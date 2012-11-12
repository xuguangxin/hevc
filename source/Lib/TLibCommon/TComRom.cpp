/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2012, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TComRom.cpp
    \brief    global variables & functions
*/

#include "TComRom.h"
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <iomanip>
#include <assert.h>
#include "TComDataCU.h"
#include "Debug.h"
// ====================================================================================================================
// Initialize / destroy functions
// ====================================================================================================================

//! \ingroup TLibCommon
//! \{

class ScanGenerator
{
private:
  UInt m_line, m_column;
  const UInt m_blockWidth, m_blockHeight;
  const UInt m_stride;
  const COEFF_SCAN_TYPE m_scanType;

public:
  ScanGenerator(UInt blockWidth, UInt blockHeight, UInt stride, COEFF_SCAN_TYPE scanType)
    : m_line(0), m_column(0), m_blockWidth(blockWidth), m_blockHeight(blockHeight), m_stride(stride), m_scanType(scanType)
  { }
  
  UInt GetCurrentX() const { return m_column; }
  UInt GetCurrentY() const { return m_line; }

  UInt GetNextIndex(UInt blockOffsetX, UInt blockOffsetY)
  {
    Int rtn=((m_line + blockOffsetY) * m_stride) + m_column + blockOffsetX;

    //advance line and column to the next position
    switch (m_scanType)
    {
      //------------------------------------------------

      case SCAN_ZIGZAG:
        {
          if (((m_line + m_column) & 0x1) == 0) //on even ranks, we go bottom-left to top-right
          {
            if (m_column == (m_blockWidth - 1)) m_line++; //at the right edge, we go down a line
            else
            {
              m_column++;
              if (m_line != 0) m_line--; //at the top edge, we just let the column advance happen and stop the line
            }
          }
          else //on odd ranks, we go top-right to bottom-left
          {
            if (m_line == (m_blockHeight - 1)) m_column++; //at the bottom edge, we go across one column
            else
            {
              m_line++;
              if (m_column != 0) m_column--; //at the left edge, we just let the line advance happen and stop the column
            }
          }
        }
        break;

      //------------------------------------------------

      case SCAN_HOR:
        {
          if (m_column == (m_blockWidth - 1))
          {
            m_line++;
            m_column = 0;
          }
          else m_column++;
        }
        break;

      //------------------------------------------------

      case SCAN_VER:
        {
          if (m_line == (m_blockHeight - 1))
          {
            m_column++;
            m_line = 0;
          }
          else m_line++;
        }
        break;

      //------------------------------------------------

      case SCAN_DIAG:
        {
          if ((m_column == (m_blockWidth - 1)) || (m_line == 0)) //if we reach the end of a rank, go diagonally down to the next one
          {
            m_line   += m_column + 1;
            m_column  = 0;

            if (m_line >= m_blockHeight) //if that takes us outside the block, adjust so that we are back on the bottom row
            {
              m_column += m_line - (m_blockHeight - 1);
              m_line    = m_blockHeight - 1;
            }
          }
          else
          {
            m_column++;
            m_line--;
          }
        }
        break;

      //------------------------------------------------

      default:
        {
          std::cerr << "ERROR: Unknown scan type \"" << m_scanType << "\"in ScanGenerator::GetNextIndex" << std::endl;
          exit(1);
        }
        break;
    }

    return rtn;
  }
};

// initialize ROM variables
Void initROM()
{
  Int i, c;

  // g_aucConvertToBit[ x ]: log2(x/4), if x=4 -> 0, x=8 -> 1, x=16 -> 2, ...
  ::memset( g_aucConvertToBit,   -1, sizeof( g_aucConvertToBit ) );
  c=0;
  for ( i=4; i<MAX_CU_SIZE; i*=2 )
  {
    g_aucConvertToBit[ i ] = c;
    c++;
  }
  g_aucConvertToBit[ i ] = c;

  // initialise scan orders
  for(UInt log2BlockHeight = 0; log2BlockHeight < MAX_CU_DEPTH; log2BlockHeight++)
  {
    for(UInt log2BlockWidth = 0; log2BlockWidth < MAX_CU_DEPTH; log2BlockWidth++)
    {
      const UInt blockWidth  = 1 << log2BlockWidth;
      const UInt blockHeight = 1 << log2BlockHeight;
      const UInt totalValues = blockWidth * blockHeight;

      //--------------------------------------------------------------------------------------------------

      //non-grouped scan orders

      for (UInt scanTypeIndex = 0; scanTypeIndex < SCAN_NUMBER_OF_TYPES; scanTypeIndex++)
      {
        const COEFF_SCAN_TYPE scanType = COEFF_SCAN_TYPE(scanTypeIndex);

        g_scanOrder[SCAN_UNGROUPED][scanType][log2BlockWidth][log2BlockHeight] = new UInt[totalValues];

        ScanGenerator fullBlockScan(blockWidth, blockHeight, blockWidth, scanType);

        for (UInt scanPosition = 0; scanPosition < totalValues; scanPosition++)
        {
          g_scanOrder[SCAN_UNGROUPED][scanType][log2BlockWidth][log2BlockHeight][scanPosition] = fullBlockScan.GetNextIndex(0, 0);
        }
      }

      //--------------------------------------------------------------------------------------------------

      //grouped scan orders

      for (UInt groupTypeIndex = SCAN_GROUPED_4x4; groupTypeIndex < SCAN_NUMBER_OF_GROUP_TYPES; groupTypeIndex++)
      {
        const COEFF_SCAN_GROUP_TYPE groupType = COEFF_SCAN_GROUP_TYPE(groupTypeIndex);

        const UInt  groupHeightScale     = groupType - 1;
        const UInt  maximumLog2GroupSize = MLS_CG_SIZE + groupHeightScale;

        const UInt  log2GroupWidth       = std::min<UInt>(log2BlockWidth,  ( maximumLog2GroupSize      >> 1)); //width rounds down and height rounds up so that, when using non-square
        const UInt  log2GroupHeight      = std::min<UInt>(log2BlockHeight, ((maximumLog2GroupSize + 1) >> 1)); //coefficient groups, the groups will be double-high rather than double-wide

        const UInt  groupWidth           = 1 << log2GroupWidth;
        const UInt  groupHeight          = 1 << log2GroupHeight;
        const UInt  widthInGroups        = blockWidth  >> log2GroupWidth;
        const UInt  heightInGroups       = blockHeight >> log2GroupHeight;

        const UInt  groupSize            = groupWidth    * groupHeight;
        const UInt  totalGroups          = widthInGroups * heightInGroups;

        for (UInt scanTypeIndex = 0; scanTypeIndex < SCAN_NUMBER_OF_TYPES; scanTypeIndex++)
        {
          const COEFF_SCAN_TYPE scanType = COEFF_SCAN_TYPE(scanTypeIndex);

          g_scanOrder[groupType][scanType][log2BlockWidth][log2BlockHeight] = new UInt[totalValues];

          ScanGenerator fullBlockScan(widthInGroups, heightInGroups, groupWidth, scanType);

          for (UInt groupIndex = 0; groupIndex < totalGroups; groupIndex++)
          {
            const UInt groupPositionY  = fullBlockScan.GetCurrentY();
            const UInt groupPositionX  = fullBlockScan.GetCurrentX();
            const UInt groupOffsetX    = groupPositionX * groupWidth;
            const UInt groupOffsetY    = groupPositionY * groupHeight;
            const UInt groupOffsetScan = groupIndex     * groupSize;

            ScanGenerator groupScan(groupWidth, groupHeight, blockWidth, scanType);

            for (UInt scanPosition = 0; scanPosition < groupSize; scanPosition++)
            {
              g_scanOrder[groupType][scanType][log2BlockWidth][log2BlockHeight][groupOffsetScan + scanPosition] = groupScan.GetNextIndex(groupOffsetX, groupOffsetY);
            }

            fullBlockScan.GetNextIndex(0,0);
          }
        }
      }

      //--------------------------------------------------------------------------------------------------
    }
  }
}

Void destroyROM()
{
  for(UInt groupTypeIndex = 0; groupTypeIndex < SCAN_NUMBER_OF_GROUP_TYPES; groupTypeIndex++)
  {
    for (UInt scanOrderIndex = 0; scanOrderIndex < SCAN_NUMBER_OF_TYPES; scanOrderIndex++)
    {
      for (UInt log2BlockWidth = 0; log2BlockWidth < MAX_CU_DEPTH; log2BlockWidth++)
      {
        for (UInt log2BlockHeight = 0; log2BlockHeight < MAX_CU_DEPTH; log2BlockHeight++)
        {
          delete [] g_scanOrder[groupTypeIndex][scanOrderIndex][log2BlockWidth][log2BlockHeight];
        }
      }
    }
  }
}

// ====================================================================================================================
// Data structure related table & variable
// ====================================================================================================================

UInt g_uiMaxCUWidth  = MAX_CU_SIZE;
UInt g_uiMaxCUHeight = MAX_CU_SIZE;
UInt g_uiMaxCUDepth  = MAX_CU_DEPTH;
UInt g_uiAddCUDepth  = 0;
UInt g_auiZscanToRaster [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ] = { 0, };
UInt g_auiRasterToZscan [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ] = { 0, };
UInt g_auiRasterToPelX  [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ] = { 0, };
UInt g_auiRasterToPelY  [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ] = { 0, };
#if !LINEBUF_CLEANUP
UInt g_motionRefer   [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ] = { 0, };
#endif

UInt g_auiPUOffset[NUMBER_OF_PART_SIZES] = { 0, 8, 4, 4, 2, 10, 1, 5};

Void initZscanToRaster ( Int iMaxDepth, Int iDepth, UInt uiStartVal, UInt*& rpuiCurrIdx )
{
  Int iStride = 1 << ( iMaxDepth - 1 );
  
  if ( iDepth == iMaxDepth )
  {
    rpuiCurrIdx[0] = uiStartVal;
    rpuiCurrIdx++;
  }
  else
  {
    Int iStep = iStride >> iDepth;
    initZscanToRaster( iMaxDepth, iDepth+1, uiStartVal,                     rpuiCurrIdx );
    initZscanToRaster( iMaxDepth, iDepth+1, uiStartVal+iStep,               rpuiCurrIdx );
    initZscanToRaster( iMaxDepth, iDepth+1, uiStartVal+iStep*iStride,       rpuiCurrIdx );
    initZscanToRaster( iMaxDepth, iDepth+1, uiStartVal+iStep*iStride+iStep, rpuiCurrIdx );
  }
}

Void initRasterToZscan ( UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uiMaxDepth )
{
  UInt  uiMinCUWidth  = uiMaxCUWidth  >> ( uiMaxDepth - 1 );
  UInt  uiMinCUHeight = uiMaxCUHeight >> ( uiMaxDepth - 1 );
  
  UInt  uiNumPartInWidth  = (UInt)uiMaxCUWidth  / uiMinCUWidth;
  UInt  uiNumPartInHeight = (UInt)uiMaxCUHeight / uiMinCUHeight;
  
  for ( UInt i = 0; i < uiNumPartInWidth*uiNumPartInHeight; i++ )
  {
    g_auiRasterToZscan[ g_auiZscanToRaster[i] ] = i;
  }
}

#if !LINEBUF_CLEANUP
/** generate motion data compression mapping table
* \param uiMaxCUWidth, width of LCU
* \param uiMaxCUHeight, hight of LCU
* \param uiMaxDepth, max depth of LCU
* \returns Void
*/
Void initMotionReferIdx ( UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uiMaxDepth )
{
  Int  minSUWidth  = (Int)uiMaxCUWidth  >> ( (Int)uiMaxDepth - 1 );
  Int  minSUHeight = (Int)uiMaxCUHeight >> ( (Int)uiMaxDepth - 1 );

  Int  numPartInWidth  = (Int)uiMaxCUWidth  / (Int)minSUWidth;
  Int  numPartInHeight = (Int)uiMaxCUHeight / (Int)minSUHeight;

  for ( Int i = 0; i < numPartInWidth*numPartInHeight; i++ )
  {
    g_motionRefer[i] = i;
  }

  UInt maxCUDepth = g_uiMaxCUDepth - ( g_uiAddCUDepth - 1);
  Int  minCUWidth  = (Int)uiMaxCUWidth  >> ( (Int)maxCUDepth - 1);

  if(!(minCUWidth == 8 && minSUWidth == 4)) //check if Minimum PU width == 4
  {
    return;
  }

  Int compressionNum = 2;

  for ( Int i = numPartInWidth*(numPartInHeight-1); i < numPartInWidth*numPartInHeight; i += compressionNum*2)
  {
    for ( Int j = 1; j < compressionNum; j++ )
    {
      g_motionRefer[g_auiRasterToZscan[i+j]] = g_auiRasterToZscan[i];
    }
  }

  for ( Int i = numPartInWidth*(numPartInHeight-1)+compressionNum*2-1; i < numPartInWidth*numPartInHeight; i += compressionNum*2)
  {
    for ( Int j = 1; j < compressionNum; j++ )
    {
      g_motionRefer[g_auiRasterToZscan[i-j]] = g_auiRasterToZscan[i];
    }
  }
}
#endif

Void initRasterToPelXY ( UInt uiMaxCUWidth, UInt uiMaxCUHeight, UInt uiMaxDepth )
{
  UInt    i;
  
  UInt* uiTempX = &g_auiRasterToPelX[0];
  UInt* uiTempY = &g_auiRasterToPelY[0];
  
  UInt  uiMinCUWidth  = uiMaxCUWidth  >> ( uiMaxDepth - 1 );
  UInt  uiMinCUHeight = uiMaxCUHeight >> ( uiMaxDepth - 1 );
  
  UInt  uiNumPartInWidth  = uiMaxCUWidth  / uiMinCUWidth;
  UInt  uiNumPartInHeight = uiMaxCUHeight / uiMinCUHeight;
  
  uiTempX[0] = 0; uiTempX++;
  for ( i = 1; i < uiNumPartInWidth; i++ )
  {
    uiTempX[0] = uiTempX[-1] + uiMinCUWidth; uiTempX++;
  }
  for ( i = 1; i < uiNumPartInHeight; i++ )
  {
    memcpy(uiTempX, uiTempX-uiNumPartInWidth, sizeof(UInt)*uiNumPartInWidth);
    uiTempX += uiNumPartInWidth;
  }
  
  for ( i = 1; i < uiNumPartInWidth*uiNumPartInHeight; i++ )
  {
    uiTempY[i] = ( i / uiNumPartInWidth ) * uiMinCUWidth;
  }
};


Int g_quantScales[SCALING_LIST_REM_NUM] =
{
  26214,23302,20560,18396,16384,14564
};

Int g_invQuantScales[SCALING_LIST_REM_NUM] =
{
  40,45,51,57,64,72
};

#ifdef ECF__EXTENDED_QP_TABLES
Int g_quantScalesInc[SCALING_LIST_REM_NUM] =
{
  26214,23302,20560,18396,16384,14564,12945,11523,10280
};

Int g_invQuantScalesInc[SCALING_LIST_REM_NUM] =
{
  40,45,51,57,64,72,81,91,102
};

Int g_quantScalesDec[SCALING_LIST_REM_NUM] =
{
  36158,32768,29127,26214,23302,20560,18396,16384,14564
};

Int g_invQuantScalesDec[SCALING_LIST_REM_NUM] =
{
  29,32,36,40,45,51,57,64,72
};
#endif

const TMatrixCoeff g_aiT4[4][4] =
{
  { 64, 64, 64, 64},
  { 83, 36,-36,-83},
  { 64,-64,-64, 64},
  { 36,-83, 83,-36}
};

const TMatrixCoeff g_aiT8[8][8] =
{
  { 64, 64, 64, 64, 64, 64, 64, 64},
  { 89, 75, 50, 18,-18,-50,-75,-89},
  { 83, 36,-36,-83,-83,-36, 36, 83},
  { 75,-18,-89,-50, 50, 89, 18,-75},
  { 64,-64,-64, 64, 64,-64,-64, 64},
  { 50,-89, 18, 75,-75,-18, 89,-50},
  { 36,-83, 83,-36,-36, 83,-83, 36},
  { 18,-50, 75,-89, 89,-75, 50,-18}
};

const TMatrixCoeff g_aiT16[16][16] =
{
  { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
  { 90, 87, 80, 70, 57, 43, 25,  9, -9,-25,-43,-57,-70,-80,-87,-90},
  { 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89},
  { 87, 57,  9,-43,-80,-90,-70,-25, 25, 70, 90, 80, 43, -9,-57,-87},
  { 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83},
  { 80,  9,-70,-87,-25, 57, 90, 43,-43,-90,-57, 25, 87, 70, -9,-80},
  { 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75},
  { 70,-43,-87,  9, 90, 25,-80,-57, 57, 80,-25,-90, -9, 87, 43,-70},
  { 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64},
  { 57,-80,-25, 90, -9,-87, 43, 70,-70,-43, 87,  9,-90, 25, 80,-57},
  { 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50},
  { 43,-90, 57, 25,-87, 70,  9,-80, 80, -9,-70, 87,-25,-57, 90,-43},
  { 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36},
  { 25,-70, 90,-80, 43,  9,-57, 87,-87, 57, -9,-43, 80,-90, 70,-25},
  { 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18},
  {  9,-25, 43,-57, 70,-80, 87,-90, 90,-87, 80,-70, 57,-43, 25, -9}
};

const TMatrixCoeff g_aiT32[32][32] =
{
  { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
  { 90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13,  4, -4,-13,-22,-31,-38,-46,-54,-61,-67,-73,-78,-82,-85,-88,-90,-90},
  { 90, 87, 80, 70, 57, 43, 25,  9, -9,-25,-43,-57,-70,-80,-87,-90,-90,-87,-80,-70,-57,-43,-25, -9,  9, 25, 43, 57, 70, 80, 87, 90},
  { 90, 82, 67, 46, 22, -4,-31,-54,-73,-85,-90,-88,-78,-61,-38,-13, 13, 38, 61, 78, 88, 90, 85, 73, 54, 31,  4,-22,-46,-67,-82,-90},
  { 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89, 89, 75, 50, 18,-18,-50,-75,-89,-89,-75,-50,-18, 18, 50, 75, 89},
  { 88, 67, 31,-13,-54,-82,-90,-78,-46, -4, 38, 73, 90, 85, 61, 22,-22,-61,-85,-90,-73,-38,  4, 46, 78, 90, 82, 54, 13,-31,-67,-88},
  { 87, 57,  9,-43,-80,-90,-70,-25, 25, 70, 90, 80, 43, -9,-57,-87,-87,-57, -9, 43, 80, 90, 70, 25,-25,-70,-90,-80,-43,  9, 57, 87},
  { 85, 46,-13,-67,-90,-73,-22, 38, 82, 88, 54, -4,-61,-90,-78,-31, 31, 78, 90, 61,  4,-54,-88,-82,-38, 22, 73, 90, 67, 13,-46,-85},
  { 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83, 83, 36,-36,-83,-83,-36, 36, 83},
  { 82, 22,-54,-90,-61, 13, 78, 85, 31,-46,-90,-67,  4, 73, 88, 38,-38,-88,-73, -4, 67, 90, 46,-31,-85,-78,-13, 61, 90, 54,-22,-82},
  { 80,  9,-70,-87,-25, 57, 90, 43,-43,-90,-57, 25, 87, 70, -9,-80,-80, -9, 70, 87, 25,-57,-90,-43, 43, 90, 57,-25,-87,-70,  9, 80},
  { 78, -4,-82,-73, 13, 85, 67,-22,-88,-61, 31, 90, 54,-38,-90,-46, 46, 90, 38,-54,-90,-31, 61, 88, 22,-67,-85,-13, 73, 82,  4,-78},
  { 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75, 75,-18,-89,-50, 50, 89, 18,-75,-75, 18, 89, 50,-50,-89,-18, 75},
  { 73,-31,-90,-22, 78, 67,-38,-90,-13, 82, 61,-46,-88, -4, 85, 54,-54,-85,  4, 88, 46,-61,-82, 13, 90, 38,-67,-78, 22, 90, 31,-73},
  { 70,-43,-87,  9, 90, 25,-80,-57, 57, 80,-25,-90, -9, 87, 43,-70,-70, 43, 87, -9,-90,-25, 80, 57,-57,-80, 25, 90,  9,-87,-43, 70},
  { 67,-54,-78, 38, 85,-22,-90,  4, 90, 13,-88,-31, 82, 46,-73,-61, 61, 73,-46,-82, 31, 88,-13,-90, -4, 90, 22,-85,-38, 78, 54,-67},
  { 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64, 64,-64,-64, 64},
  { 61,-73,-46, 82, 31,-88,-13, 90, -4,-90, 22, 85,-38,-78, 54, 67,-67,-54, 78, 38,-85,-22, 90,  4,-90, 13, 88,-31,-82, 46, 73,-61},
  { 57,-80,-25, 90, -9,-87, 43, 70,-70,-43, 87,  9,-90, 25, 80,-57,-57, 80, 25,-90,  9, 87,-43,-70, 70, 43,-87, -9, 90,-25,-80, 57},
  { 54,-85, -4, 88,-46,-61, 82, 13,-90, 38, 67,-78,-22, 90,-31,-73, 73, 31,-90, 22, 78,-67,-38, 90,-13,-82, 61, 46,-88,  4, 85,-54},
  { 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50, 50,-89, 18, 75,-75,-18, 89,-50,-50, 89,-18,-75, 75, 18,-89, 50},
  { 46,-90, 38, 54,-90, 31, 61,-88, 22, 67,-85, 13, 73,-82,  4, 78,-78, -4, 82,-73,-13, 85,-67,-22, 88,-61,-31, 90,-54,-38, 90,-46},
  { 43,-90, 57, 25,-87, 70,  9,-80, 80, -9,-70, 87,-25,-57, 90,-43,-43, 90,-57,-25, 87,-70, -9, 80,-80,  9, 70,-87, 25, 57,-90, 43},
  { 38,-88, 73, -4,-67, 90,-46,-31, 85,-78, 13, 61,-90, 54, 22,-82, 82,-22,-54, 90,-61,-13, 78,-85, 31, 46,-90, 67,  4,-73, 88,-38},
  { 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36, 36,-83, 83,-36,-36, 83,-83, 36},
  { 31,-78, 90,-61,  4, 54,-88, 82,-38,-22, 73,-90, 67,-13,-46, 85,-85, 46, 13,-67, 90,-73, 22, 38,-82, 88,-54, -4, 61,-90, 78,-31},
  { 25,-70, 90,-80, 43,  9,-57, 87,-87, 57, -9,-43, 80,-90, 70,-25,-25, 70,-90, 80,-43, -9, 57,-87, 87,-57,  9, 43,-80, 90,-70, 25},
  { 22,-61, 85,-90, 73,-38, -4, 46,-78, 90,-82, 54,-13,-31, 67,-88, 88,-67, 31, 13,-54, 82,-90, 78,-46,  4, 38,-73, 90,-85, 61,-22},
  { 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18, 18,-50, 75,-89, 89,-75, 50,-18,-18, 50,-75, 89,-89, 75,-50, 18},
  { 13,-38, 61,-78, 88,-90, 85,-73, 54,-31,  4, 22,-46, 67,-82, 90,-90, 82,-67, 46,-22, -4, 31,-54, 73,-85, 90,-88, 78,-61, 38,-13},
  {  9,-25, 43,-57, 70,-80, 87,-90, 90,-87, 80,-70, 57,-43, 25, -9, -9, 25,-43, 57,-70, 80,-87, 90,-90, 87,-80, 70,-57, 43,-25,  9},
  {  4,-13, 22,-31, 38,-46, 54,-61, 67,-73, 78,-82, 85,-88, 90,-90, 90,-90, 88,-85, 82,-78, 73,-67, 61,-54, 46,-38, 31,-22, 13, -4}
};

const UChar g_aucChromaScale[chromaQPMappingTableSize]=
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,29,30,31,32,33,33,34,34,35,35,36,36,37,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
};

#ifdef ECF__MULTIPLE_CHROMA_QP_MAPPING_TABLES

const UChar g_aucChromaScale422[chromaQPMappingTableSize]=
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,32,33,34,35,36,37,37,38,39,40,40,41,42,42,43,44,44,45,45,46,47,48,49,50,51
};

const UChar g_aucChromaScale444[chromaQPMappingTableSize]=
{
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,51,51,51,51,51,51
};

#endif


// Mode-Dependent DCT/DST 
const TMatrixCoeff g_as_DST_MAT_4 [4][4]=
{
  {29,   55,    74,   84},
  {74,   74,    0 ,  -74},
  {84,  -29,   -74,   55},
  {55,  -84,    74,  -29},
};


// ====================================================================================================================
// ADI
// ====================================================================================================================

#if FAST_UDI_USE_MPM
const UChar g_aucIntraModeNumFast[MAX_CU_DEPTH] =
{
  3,  //   2x2
  8,  //   4x4
  8,  //   8x8
  3,  //  16x16
  3,  //  32x32
  3   //  64x64
#if (ECF__BACKWARDS_COMPATIBILITY_HM != 0)
 ,3   // 128x128
#endif
};
#else // FAST_UDI_USE_MPM
const UChar g_aucIntraModeNumFast[MAX_CU_DEPTH] =
{
  3,  //   2x2
  9,  //   4x4
  9,  //   8x8
  4,  //  16x16   33
  4,  //  32x32   33
  5   //  64x64   33
#if (ECF__BACKWARDS_COMPATIBILITY_HM != 0)
 ,4   // 128x128  33
#endif
};
#endif // FAST_UDI_USE_MPM


// ====================================================================================================================
// Bit-depth
// ====================================================================================================================

Int g_bitDepth   [MAX_NUM_CHANNEL_TYPE] = {8, 8};
Int g_PCMBitDepth[MAX_NUM_CHANNEL_TYPE] = {8, 8};    // PCM bit-depth

// ====================================================================================================================
// Misc.
// ====================================================================================================================

Char  g_aucConvertToBit  [ MAX_CU_SIZE+1 ];

#if ENC_DEC_TRACE
FILE*  g_hTrace = stdout; // Set to NULL to open up a file. Set to stdout to use the current output
const Bool g_bEncDecTraceEnable  = true;
const Bool g_bEncDecTraceDisable = false;
Bool   g_HLSTraceEnable = true;
Bool   g_bJustDoIt = false;
UInt64 g_nSymbolCounter = 0;
#endif
// ====================================================================================================================
// Scanning order & context model mapping
// ====================================================================================================================

// scanning order table
UInt* g_scanOrder[SCAN_NUMBER_OF_GROUP_TYPES][SCAN_NUMBER_OF_TYPES][ MAX_CU_DEPTH ][ MAX_CU_DEPTH ];

const UInt ctxIndMap4x4[4*4] =
{
  0, 1, 4, 5,
  2, 3, 4, 5,
  6, 6, 8, 8,
  7, 7, 8, 8
};

#ifdef ECF__EXTENDED_CHROMA_SIGNIFICANCE_MAP_CONTEXT

const UInt ctxIndMap4x8[4*8] =
{
  0, 1, 4, 5,
  2, 1, 4, 5,
  2, 3, 4, 5,
  2, 3, 4, 5,
  6, 6, 8, 8,
  6, 6, 8, 8,
  7, 7, 8, 8,
  7, 7, 8, 8
};

const UInt ctxIndMap8x4[8*4] =
{
  0, 1, 1, 1, 4, 4, 5, 5,
  2, 2, 3, 3, 4, 4, 5, 5,
  6, 6, 6, 6, 8, 8, 8, 8,
  7, 7, 7, 7, 8, 8, 8, 8
};

#endif

const UInt g_uiMinInGroup[ LAST_SIGNIFICANT_GROUPS ] = {0,1,2,3,4,6,8,12,16,24};
const UInt g_uiGroupIdx[ MAX_TU_SIZE ]   = {0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9};

// Rice parameters for absolute transform levels
const UInt g_auiGoRiceRange[MAXIMUM_GOLOMB_RICE_PARAMETER] =
{
  7, 14, 26, 46, 78
};

const UInt g_auiGoRicePrefixLen[MAXIMUM_GOLOMB_RICE_PARAMETER] =
{
  8, 7, 6, 5, 4
};

const char *MatrixType[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM] =
{
  {
  "INTRA4X4_LUMA",
  "INTRA4X4_CHROMAU",
  "INTRA4X4_CHROMAV",
  "INTER4X4_LUMA",
  "INTER4X4_CHROMAU",
  "INTER4X4_CHROMAV"
  },
  {
  "INTRA8X8_LUMA",
  "INTRA8X8_CHROMAU", 
  "INTRA8X8_CHROMAV", 
  "INTER8X8_LUMA",
  "INTER8X8_CHROMAU", 
  "INTER8X8_CHROMAV"  
  },
  {
  "INTRA16X16_LUMA",
  "INTRA16X16_CHROMAU", 
  "INTRA16X16_CHROMAV", 
  "INTER16X16_LUMA",
  "INTER16X16_CHROMAU", 
  "INTER16X16_CHROMAV"  
  },
  {
  "INTRA32X32_LUMA",
  "INTER32X32_LUMA",
  },
};

const char *MatrixType_DC[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM] =
{
  {
  },
  {
  },
  {
  "INTRA16X16_LUMA_DC",
  "INTRA16X16_CHROMAU_DC", 
  "INTRA16X16_CHROMAV_DC", 
  "INTER16X16_LUMA_DC",
  "INTER16X16_CHROMAU_DC", 
  "INTER16X16_CHROMAV_DC"  
  },
  {
  "INTRA32X32_LUMA_DC",
  "INTER32X32_LUMA_DC",
  },
};

#if !FLAT_4x4_DSL
Int g_quantIntraDefault4x4[4*4] =
{
  16,16,17,21,
  16,17,20,25,
  17,20,30,41,
  21,25,41,70
};
Int g_quantInterDefault4x4[4*4] =
{
  16,16,17,21,
  16,17,21,24,
  17,21,24,36,
  21,24,36,57
};
#endif
Int g_quantTSDefault4x4[4*4] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

Int g_quantIntraDefault8x8[8*8] =
{
  16,16,16,16,17,18,21,24,
  16,16,16,16,17,19,22,25,
  16,16,17,18,20,22,25,29,
  16,16,18,21,24,27,31,36,
  17,17,20,24,30,35,41,47,
  18,19,22,27,35,44,54,65,
  21,22,25,31,41,54,70,88,
  24,25,29,36,47,65,88,115
};

Int g_quantInterDefault8x8[8*8] =
{
  16,16,16,16,17,18,20,24,
  16,16,16,17,18,20,24,25,
  16,16,17,18,20,24,25,28,
  16,17,18,20,24,25,28,33,
  17,18,20,24,25,28,33,41,
  18,20,24,25,28,33,41,54,
  20,24,25,28,33,41,54,71,
  24,25,28,33,41,54,71,91
};

UInt g_scalingListSize   [SCALING_LIST_SIZE_NUM] = {16,64,256,1024};
UInt g_scalingListSizeX  [SCALING_LIST_SIZE_NUM] = { 4, 8, 16,  32};
#if ECF__INCREASE_NUMBER_OF_SCALING_LISTS_FOR_CHROMA
UInt g_scalingListNum[SCALING_LIST_SIZE_NUM]={SCALING_LIST_NUM, SCALING_LIST_NUM, SCALING_LIST_NUM, SCALING_LIST_NUM};
#else
UInt g_scalingListNum[SCALING_LIST_SIZE_NUM]={SCALING_LIST_NUM, SCALING_LIST_NUM, SCALING_LIST_NUM, NUMBER_OF_PREDICTION_MODES};
#endif
//! \}
