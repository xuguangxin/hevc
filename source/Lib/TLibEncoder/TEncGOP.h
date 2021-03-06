/* ====================================================================================================================

  The copyright in this software is being made available under the License included below.
  This software may be subject to other third party and   contributor rights, including patent rights, and no such
  rights are granted under this license.

  Copyright (c) 2010, SAMSUNG ELECTRONICS CO., LTD. and BRITISH BROADCASTING CORPORATION
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted only for
  the purpose of developing standards within the Joint Collaborative Team on Video Coding and for testing and
  promoting such standards. The following conditions are required to be met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
      the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
      the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of SAMSUNG ELECTRONICS CO., LTD. nor the name of the BRITISH BROADCASTING CORPORATION
      may be used to endorse or promote products derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 * ====================================================================================================================
*/

/** \file     TEncGOP.h
    \brief    GOP encoder class (header)
*/

#ifndef __TENCGOP__
#define __TENCGOP__

#include <stdlib.h>

#include "../TLibCommon/TComList.h"
#include "../TLibCommon/TComPic.h"
#include "../TLibCommon/TComBitStream.h"
#include "../TLibCommon/TComBitCounter.h"
#include "../TLibCommon/TComLoopFilter.h"
#include "TEncAdaptiveLoopFilter.h"
#include "TEncSlice.h"
#include "TEncEntropy.h"
#include "TEncCavlc.h"
#include "TEncSbac.h"

#include "TEncAnalyze.h"
#ifdef QC_SIFO
#include "TEncSIFO.h"
#endif

class TEncTop;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// GOP encoder class
class TEncGOP
{
private:
  //  Data
  Int                     m_iHrchDepth;
  Int                     m_iGopSize;
  Int                     m_iRateGopSize;
  Int                     m_iNumPicCoded;
  Bool                    m_bFirst;
  UInt                    m_uiBalancedCPUs;

  //  Access channel
  TEncTop*                m_pcEncTop;
  TEncCfg*                m_pcCfg;
  TEncSlice*              m_pcSliceEncoder;
#ifdef QC_SIFO
  TEncSIFO*               m_pcSIFOEncoder;
#endif
  TComList<TComPic*>*     m_pcListPic;

  TEncEntropy*            m_pcEntropyCoder;
  TEncCavlc*              m_pcCavlcCoder;
  TEncSbac*               m_pcSbacCoder;
  TEncBinCABAC*           m_pcBinCABAC;
  TEncBinMultiCABAC*      m_pcBinMultiCABAC;
  TEncBinPIPE*            m_pcBinPIPE;
  TEncBinMultiPIPE*       m_pcBinMultiPIPE;
  TEncV2V*                m_pcBinV2VwLB;
  TComLoopFilter*         m_pcLoopFilter;
  TEncBinCABAC4V2V*       m_pcBinCABAC4V2V;

  // Adaptive Loop filter
  TEncAdaptiveLoopFilter* m_pcAdaptiveLoopFilter;
  //--Adaptive Loop filter

  TComBitCounter*         m_pcBitCounter;

  // indicate sequence first
  Bool                    m_bSeqFirst;

public:
  TEncGOP();
  virtual ~TEncGOP();

  Void  create      ();
  Void  destroy     ();

  Void setBalancedCPUs( UInt u ) { m_uiBalancedCPUs = u; }
  UInt getBalancedCPUs()         { return m_uiBalancedCPUs; }

  Void  init        ( TEncTop* pcTEncTop );
  Void  compressGOP ( Int iPOCLast, Int iNumPicRcvd, TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRec, TComList<TComBitstream*> rcListBitstream );

  Int   getGOPSize()          { return  m_iGopSize;  }
  Int   getRateGOPSize()      { return  m_iRateGopSize;  }
  Int   isHierarchicalB()     { return  m_pcCfg->getHierarchicalCoding();  }
  Int   getHrchDepth()        { return  m_iHrchDepth; }

  TComList<TComPic*>*   getListPic()      { return m_pcListPic; }

  Void  printOutSummary      ( UInt uiNumAllPicCoded );
  Void  preLoopFilterPicAll  ( TComPic* pcPic, UInt64& ruiDist, UInt64& ruiBits );

  TEncSlice*  getSliceEncoder()   { return m_pcSliceEncoder; }

  Void setCABAC4V2V (TEncBinCABAC4V2V* p) { m_pcBinCABAC4V2V = p; }

protected:
  Void  xInitGOP          ( Int iPOC, Int iNumPicRcvd, TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRecOut );
  Void  xGetBuffer        ( TComList<TComPic*>& rcListPic, TComList<TComPicYuv*>& rcListPicYuvRecOut, TComList<TComBitstream*>& rcListBitstream, Int iNumPicRcvd, Int iTimeOffset, TComPic*& rpcPic, TComPicYuv*& rpcPicYuvRecOut, TComBitstream*& rpcBitstreamOut, UInt uiPOCCurr );

  // for scaling & descaing of picture
  // note: IBDI is handled here
  Void  xScalePic         ( TComPic* pcPic );
  Void  xDeScalePic       ( TComPic* pcPic, TComPicYuv* pcPicD );

  Void  xCalculateAddPSNR ( TComPic* pcPic, TComPicYuv* pcPicD, UInt uiBits );
  Void  xCalculateAddPSNR ( TComPic* pcPic, TComPicYuv* pcPicD, UInt uiBits, Double dEncTime );

  UInt64 xFindDistortionFrame (TComPicYuv* pcPic0, TComPicYuv* pcPic1);
};// END CLASS DEFINITION TEncGOP


#endif // __TENCGOP__

