/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2014, ITU/ISO/IEC
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

/**
 \file     SEIread.cpp
 \brief    reading funtionality for SEI messages
 */

#include "TLibCommon/CommonDef.h"
#include "TLibCommon/TComBitStream.h"
#include "TLibCommon/SEI.h"
#include "TLibCommon/TComSlice.h"
#include "SyntaxElementParser.h"
#include "SEIread.h"
#include "TLibCommon/TComPicYuv.h"

//! \ingroup TLibDecoder
//! \{

#if ENC_DEC_TRACE
Void  xTraceSEIHeader()
{
  fprintf( g_hTrace, "=========== SEI message ===========\n");
}

Void  xTraceSEIMessageType(SEI::PayloadType payloadType)
{
  switch (payloadType)
  {
  case SEI::DECODED_PICTURE_HASH:
    fprintf( g_hTrace, "=========== Decoded picture hash SEI message ===========\n");
    break;
  case SEI::USER_DATA_UNREGISTERED:
    fprintf( g_hTrace, "=========== User Data Unregistered SEI message ===========\n");
    break;
  case SEI::ACTIVE_PARAMETER_SETS:
    fprintf( g_hTrace, "=========== Active Parameter Sets SEI message ===========\n");
    break;
  case SEI::BUFFERING_PERIOD:
    fprintf( g_hTrace, "=========== Buffering period SEI message ===========\n");
    break;
  case SEI::PICTURE_TIMING:
    fprintf( g_hTrace, "=========== Picture timing SEI message ===========\n");
    break;
  case SEI::RECOVERY_POINT:
    fprintf( g_hTrace, "=========== Recovery point SEI message ===========\n");
    break;
  case SEI::FRAME_PACKING:
    fprintf( g_hTrace, "=========== Frame Packing Arrangement SEI message ===========\n");
    break;
  case SEI::DISPLAY_ORIENTATION:
    fprintf( g_hTrace, "=========== Display Orientation SEI message ===========\n");
    break;
  case SEI::TEMPORAL_LEVEL0_INDEX:
    fprintf( g_hTrace, "=========== Temporal Level Zero Index SEI message ===========\n");
    break;
  case SEI::REGION_REFRESH_INFO:
    fprintf( g_hTrace, "=========== Gradual Decoding Refresh Information SEI message ===========\n");
    break;
  case SEI::NO_DISPLAY:
    fprintf( g_hTrace, "=========== No Display SEI message ===========\n");
    break;
  case SEI::DECODING_UNIT_INFO:
    fprintf( g_hTrace, "=========== Decoding Unit Information SEI message ===========\n");
    break;
  case SEI::TONE_MAPPING_INFO:
    fprintf( g_hTrace, "===========Tone Mapping Info SEI message ===========\n");
    break;
  case SEI::SOP_DESCRIPTION:
    fprintf( g_hTrace, "=========== SOP Description SEI message ===========\n");
    break;
  case SEI::SCALABLE_NESTING:
    fprintf( g_hTrace, "=========== Scalable Nesting SEI message ===========\n");
    break;
#if RExt__O0099_TIME_CODE_SEI
  case SEI::TIME_CODE:
      fprintf( g_hTrace, "=========== Time Code SEI message ===========\n");
      break;
#endif
#if RExt__P0084_MASTERING_DISPLAY_COLOUR_VOLUME_SEI
  case SEI::MASTERING_DISPLAY_COLOUR_VOLUME:
    fprintf( g_hTrace, "=========== Mastering Display Colour Volume SEI message ===========\n");
    break;
#endif
  case SEI::KNEE_FUNCTION_INFO:
    fprintf( g_hTrace, "=========== Knee Function Information SEI message ===========\n");
    break;
  default:
    fprintf( g_hTrace, "=========== Unknown SEI message ===========\n");
    break;
  }
}
#endif

/**
 * unmarshal a single SEI message from bitstream bs
 */
void SEIReader::parseSEImessage(TComInputBitstream* bs, SEIMessages& seis, const NalUnitType nalUnitType, TComSPS *sps)
{
  setBitstream(bs);

  assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  do
  {
    xReadSEImessage(seis, nalUnitType, sps);

    /* SEI messages are an integer number of bytes, something has failed
    * in the parsing if bitstream not byte-aligned */
    assert(!m_pcBitstream->getNumBitsUntilByteAligned());
  } while (m_pcBitstream->getNumBitsLeft() > 8);

  UInt rbspTrailingBits;
  READ_CODE(8, rbspTrailingBits, "rbsp_trailing_bits");
  assert(rbspTrailingBits == 0x80);
}

Void SEIReader::xReadSEImessage(SEIMessages& seis, const NalUnitType nalUnitType, TComSPS *sps)
{
#if ENC_DEC_TRACE
  xTraceSEIHeader();
#endif
  Int payloadType = 0;
  UInt val = 0;

  do
  {
    READ_CODE (8, val, "payload_type");
    payloadType += val;
  } while (val==0xFF);

  UInt payloadSize = 0;
  do
  {
    READ_CODE (8, val, "payload_size");
    payloadSize += val;
  } while (val==0xFF);

#if ENC_DEC_TRACE
  xTraceSEIMessageType((SEI::PayloadType)payloadType);
#endif

  /* extract the payload for this single SEI message.
   * This allows greater safety in erroneous parsing of an SEI message
   * from affecting subsequent messages.
   * After parsing the payload, bs needs to be restored as the primary
   * bitstream.
   */
  TComInputBitstream *bs = getBitstream();
  setBitstream(bs->extractSubstream(payloadSize * 8));

  SEI *sei = NULL;

  if(nalUnitType == NAL_UNIT_PREFIX_SEI)
  {
    switch (payloadType)
    {
    case SEI::USER_DATA_UNREGISTERED:
      sei = new SEIuserDataUnregistered;
      xParseSEIuserDataUnregistered((SEIuserDataUnregistered&) *sei, payloadSize);
      break;
    case SEI::ACTIVE_PARAMETER_SETS:
      sei = new SEIActiveParameterSets;
      xParseSEIActiveParameterSets((SEIActiveParameterSets&) *sei, payloadSize);
      break;
    case SEI::DECODING_UNIT_INFO:
      if (!sps)
      {
        printf ("Warning: Found Decoding unit SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIDecodingUnitInfo;
        xParseSEIDecodingUnitInfo((SEIDecodingUnitInfo&) *sei, payloadSize, sps);
      }
      break;
    case SEI::BUFFERING_PERIOD:
      if (!sps)
      {
        printf ("Warning: Found Buffering period SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIBufferingPeriod;
        xParseSEIBufferingPeriod((SEIBufferingPeriod&) *sei, payloadSize, sps);
      }
      break;
    case SEI::PICTURE_TIMING:
      if (!sps)
      {
        printf ("Warning: Found Picture timing SEI message, but no active SPS is available. Ignoring.");
      }
      else
      {
        sei = new SEIPictureTiming;
        xParseSEIPictureTiming((SEIPictureTiming&)*sei, payloadSize, sps);
      }
      break;
    case SEI::RECOVERY_POINT:
      sei = new SEIRecoveryPoint;
      xParseSEIRecoveryPoint((SEIRecoveryPoint&) *sei, payloadSize);
      break;
    case SEI::FRAME_PACKING:
      sei = new SEIFramePacking;
      xParseSEIFramePacking((SEIFramePacking&) *sei, payloadSize);
      break;
    case SEI::DISPLAY_ORIENTATION:
      sei = new SEIDisplayOrientation;
      xParseSEIDisplayOrientation((SEIDisplayOrientation&) *sei, payloadSize);
      break;
    case SEI::TEMPORAL_LEVEL0_INDEX:
      sei = new SEITemporalLevel0Index;
      xParseSEITemporalLevel0Index((SEITemporalLevel0Index&) *sei, payloadSize);
      break;
    case SEI::REGION_REFRESH_INFO:
      sei = new SEIGradualDecodingRefreshInfo;
      xParseSEIGradualDecodingRefreshInfo((SEIGradualDecodingRefreshInfo&) *sei, payloadSize);
      break;
    case SEI::NO_DISPLAY:
      sei = new SEINoDisplay;
      xParseSEINoDisplay((SEINoDisplay&) *sei, payloadSize);
      break;
    case SEI::TONE_MAPPING_INFO:
      sei = new SEIToneMappingInfo;
      xParseSEIToneMappingInfo((SEIToneMappingInfo&) *sei, payloadSize);
      break;
    case SEI::SOP_DESCRIPTION:
      sei = new SEISOPDescription;
      xParseSEISOPDescription((SEISOPDescription&) *sei, payloadSize);
      break;
    case SEI::SCALABLE_NESTING:
      sei = new SEIScalableNesting;
      xParseSEIScalableNesting((SEIScalableNesting&) *sei, nalUnitType, payloadSize, sps);
      break;
#if RExt__O0099_TIME_CODE_SEI
    case SEI::TIME_CODE:
      sei = new SEITimeCode;
      xParseSEITimeCode((SEITimeCode&) *sei, payloadSize);
      break;
#endif   
#if RExt__O0079_CHROMA_SAMPLING_FILTER_HINT_SEI
    case SEI::CHROMA_SAMPLING_FILTER_HINT:
      //NOTE: RExt - Made unconditional on SPS to be consistent with the working text P1005
      //if (!sps)
      //{
      //  printf ("Warning: Found Chroma Sampling Filter Hint SEI message, but no active SPS is available. Ignoring.\n");
      //}
      //else
      //{
      sei = new SEIChromaSamplingFilterHint;
      xParseSEIChromaSamplingFilterHint((SEIChromaSamplingFilterHint&) *sei, payloadSize/*, sps*/);
      //}
      break;
#endif
    case SEI::KNEE_FUNCTION_INFO:
      sei = new SEIKneeFunctionInfo;
      xParseSEIKneeFunctionInfo((SEIKneeFunctionInfo&) *sei, payloadSize);
      break;
#if RExt__P0084_MASTERING_DISPLAY_COLOUR_VOLUME_SEI
    case SEI::MASTERING_DISPLAY_COLOUR_VOLUME:
      sei = new SEIMasteringDisplayColourVolume;
      xParseSEIMasteringDisplayColourVolume((SEIMasteringDisplayColourVolume&) *sei, payloadSize);
      break;
#endif
    default:
      for (UInt i = 0; i < payloadSize; i++)
      {
        UInt seiByte;
        READ_CODE (8, seiByte, "unknown prefix SEI payload byte");
      }
      printf ("Unknown prefix SEI message (payloadType = %d) was found!\n", payloadType);
    }
  }
  else
  {
    switch (payloadType)
    {
      case SEI::USER_DATA_UNREGISTERED:
        sei = new SEIuserDataUnregistered;
        xParseSEIuserDataUnregistered((SEIuserDataUnregistered&) *sei, payloadSize);
        break;
      case SEI::DECODED_PICTURE_HASH:
        sei = new SEIDecodedPictureHash;
        xParseSEIDecodedPictureHash((SEIDecodedPictureHash&) *sei, payloadSize);
        break;
      default:
        for (UInt i = 0; i < payloadSize; i++)
        {
          UInt seiByte;
          READ_CODE (8, seiByte, "unknown suffix SEI payload byte");
        }
        printf ("Unknown suffix SEI message (payloadType = %d) was found!\n", payloadType);
    }
  }

  if (sei != NULL)
  {
    seis.push_back(sei);
  }

  /* By definition the underlying bitstream terminates in a byte-aligned manner.
   * 1. Extract all bar the last MIN(bitsremaining,nine) bits as reserved_payload_extension_data
   * 2. Examine the final 8 bits to determine the payload_bit_equal_to_one marker
   * 3. Extract the remainingreserved_payload_extension_data bits.
   *
   * If there are fewer than 9 bits available, extract them.
   */
  Int payloadBitsRemaining = getBitstream()->getNumBitsLeft();
  if (payloadBitsRemaining) /* more_data_in_payload() */
  {
    for (; payloadBitsRemaining > 9; payloadBitsRemaining--)
    {
      UInt reservedPayloadExtensionData;
      READ_CODE (1, reservedPayloadExtensionData, "reserved_payload_extension_data");
    }

    /* 2 */
    Int finalBits = getBitstream()->peekBits(payloadBitsRemaining);
    Int finalPayloadBits = 0;
    for (Int mask = 0xff; finalBits & (mask >> finalPayloadBits); finalPayloadBits++)
    {
      continue;
    }

    /* 3 */
    for (; payloadBitsRemaining > 9 - finalPayloadBits; payloadBitsRemaining--)
    {
      UInt reservedPayloadExtensionData;
      READ_FLAG (reservedPayloadExtensionData, "reserved_payload_extension_data");
    }

    UInt dummy;
    READ_FLAG (dummy, "payload_bit_equal_to_one"); payloadBitsRemaining--;
    while (payloadBitsRemaining)
    {
      READ_FLAG (dummy, "payload_bit_equal_to_zero"); payloadBitsRemaining--;
    }
  }

  /* restore primary bitstream for sei_message */
  getBitstream()->deleteFifo();
  delete getBitstream();
  setBitstream(bs);
}

/**
 * parse bitstream bs and unpack a user_data_unregistered SEI message
 * of payloasSize bytes into sei.
 */
Void SEIReader::xParseSEIuserDataUnregistered(SEIuserDataUnregistered &sei, UInt payloadSize)
{
  assert(payloadSize >= ISO_IEC_11578_LEN);
  UInt val;

  for (UInt i = 0; i < ISO_IEC_11578_LEN; i++)
  {
    READ_CODE (8, val, "uuid_iso_iec_11578");
    sei.uuid_iso_iec_11578[i] = val;
  }

  sei.userDataLength = payloadSize - ISO_IEC_11578_LEN;
  if (!sei.userDataLength)
  {
    sei.userData = 0;
    return;
  }

  sei.userData = new UChar[sei.userDataLength];
  for (UInt i = 0; i < sei.userDataLength; i++)
  {
    READ_CODE (8, val, "user_data" );
    sei.userData[i] = val;
  }
}

/**
 * parse bitstream bs and unpack a decoded picture hash SEI message
 * of payloadSize bytes into sei.
 */
Void SEIReader::xParseSEIDecodedPictureHash(SEIDecodedPictureHash& sei, UInt payloadSize)
{
  UInt bytesRead = 0;

  UInt val;
  READ_CODE (8, val, "hash_type");
  sei.method = static_cast<SEIDecodedPictureHash::Method>(val); bytesRead++;

  const Char *traceString="\0";
  switch (sei.method)
  {
    case SEIDecodedPictureHash::MD5: traceString="picture_md5"; break;
    case SEIDecodedPictureHash::CRC: traceString="picture_crc"; break;
    case SEIDecodedPictureHash::CHECKSUM: traceString="picture_checksum"; break;
    default: assert(false); break;
  }

  if (traceString != 0) //use of this variable is needed to avoid a compiler error with G++ 4.6.1
  {
    sei.m_digest.hash.clear();
    for(;bytesRead < payloadSize; bytesRead++)
    {
      READ_CODE (8, val, traceString);
      sei.m_digest.hash.push_back((unsigned char)val);
    }
  }
}

Void SEIReader::xParseSEIActiveParameterSets(SEIActiveParameterSets& sei, UInt /*payloadSize*/)
{
  UInt val;
  READ_CODE(4, val, "active_vps_id");      sei.activeVPSId = val;
  READ_FLAG( val, "full_random_access_flag");  sei.m_fullRandomAccessFlag = (val != 0);
  READ_FLAG( val, "no_param_set_update_flag"); sei.m_noParamSetUpdateFlag = (val != 0);
  READ_UVLC(   val, "num_sps_ids_minus1"); sei.numSpsIdsMinus1 = val;

  sei.activeSeqParamSetId.resize(sei.numSpsIdsMinus1 + 1);
  for (Int i=0; i < (sei.numSpsIdsMinus1 + 1); i++)
  {
    READ_UVLC(val, "active_seq_param_set_id");  sei.activeSeqParamSetId[i] = val;
  }

  UInt uibits = m_pcBitstream->getNumBitsUntilByteAligned();

  while(uibits--)
  {
    READ_FLAG(val, "alignment_bit");
  }
}

Void SEIReader::xParseSEIDecodingUnitInfo(SEIDecodingUnitInfo& sei, UInt /*payloadSize*/, TComSPS *sps)
{
  UInt val;
  READ_UVLC(val, "decoding_unit_idx");
  sei.m_decodingUnitIdx = val;

  TComVUI *vui = sps->getVuiParameters();
  if(vui->getHrdParameters()->getSubPicCpbParamsInPicTimingSEIFlag())
  {
    READ_CODE( ( vui->getHrdParameters()->getDuCpbRemovalDelayLengthMinus1() + 1 ), val, "du_spt_cpb_removal_delay");
    sei.m_duSptCpbRemovalDelay = val;
  }
  else
  {
    sei.m_duSptCpbRemovalDelay = 0;
  }
  READ_FLAG( val, "dpb_output_du_delay_present_flag"); sei.m_dpbOutputDuDelayPresentFlag = (val != 0);
  if(sei.m_dpbOutputDuDelayPresentFlag)
  {
    READ_CODE(vui->getHrdParameters()->getDpbOutputDelayDuLengthMinus1() + 1, val, "pic_spt_dpb_output_du_delay");
    sei.m_picSptDpbOutputDuDelay = val;
  }
  xParseByteAlign();
}

Void SEIReader::xParseSEIBufferingPeriod(SEIBufferingPeriod& sei, UInt /*payloadSize*/, TComSPS *sps)
{
  Int i, nalOrVcl;
  UInt code;

  TComVUI *pVUI = sps->getVuiParameters();
  TComHRD *pHRD = pVUI->getHrdParameters();

  READ_UVLC( code, "bp_seq_parameter_set_id" );                         sei.m_bpSeqParameterSetId     = code;
  if( !pHRD->getSubPicCpbParamsPresentFlag() )
  {
    READ_FLAG( code, "irap_cpb_params_present_flag" );                   sei.m_rapCpbParamsPresentFlag = code;
  }
  if( sei.m_rapCpbParamsPresentFlag )
  {
    READ_CODE( pHRD->getCpbRemovalDelayLengthMinus1() + 1, code, "cpb_delay_offset" );      sei.m_cpbDelayOffset = code;
    READ_CODE( pHRD->getDpbOutputDelayLengthMinus1()  + 1, code, "dpb_delay_offset" );      sei.m_dpbDelayOffset = code;
  }

  //read splicing flag and cpb_removal_delay_delta
  READ_FLAG( code, "concatenation_flag");
  sei.m_concatenationFlag = code;
  READ_CODE( ( pHRD->getCpbRemovalDelayLengthMinus1() + 1 ), code, "au_cpb_removal_delay_delta_minus1" );
  sei.m_auCpbRemovalDelayDelta = code + 1;

  for( nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
  {
    if( ( ( nalOrVcl == 0 ) && ( pHRD->getNalHrdParametersPresentFlag() ) ) ||
        ( ( nalOrVcl == 1 ) && ( pHRD->getVclHrdParametersPresentFlag() ) ) )
    {
      for( i = 0; i < ( pHRD->getCpbCntMinus1( 0 ) + 1 ); i ++ )
      {
        READ_CODE( ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_cpb_removal_delay" );
        sei.m_initialCpbRemovalDelay[i][nalOrVcl] = code;
        READ_CODE( ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_cpb_removal_delay_offset" );
        sei.m_initialCpbRemovalDelayOffset[i][nalOrVcl] = code;
        if( pHRD->getSubPicCpbParamsPresentFlag() || sei.m_rapCpbParamsPresentFlag )
        {
          READ_CODE( ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_alt_cpb_removal_delay" );
          sei.m_initialAltCpbRemovalDelay[i][nalOrVcl] = code;
          READ_CODE( ( pHRD->getInitialCpbRemovalDelayLengthMinus1() + 1 ) , code, "initial_alt_cpb_removal_delay_offset" );
          sei.m_initialAltCpbRemovalDelayOffset[i][nalOrVcl] = code;
        }
      }
    }
  }
  xParseByteAlign();
}

Void SEIReader::xParseSEIPictureTiming(SEIPictureTiming& sei, UInt /*payloadSize*/, TComSPS *sps)
{
  Int i;
  UInt code;

  TComVUI *vui = sps->getVuiParameters();
  TComHRD *hrd = vui->getHrdParameters();

  if( vui->getFrameFieldInfoPresentFlag() )
  {
    READ_CODE( 4, code, "pic_struct" );             sei.m_picStruct            = code;
    READ_CODE( 2, code, "source_scan_type" );       sei.m_sourceScanType       = code;
    READ_FLAG(    code, "duplicate_flag" );         sei.m_duplicateFlag        = (code == 1);
  }

  if( hrd->getCpbDpbDelaysPresentFlag())
  {
    READ_CODE( ( hrd->getCpbRemovalDelayLengthMinus1() + 1 ), code, "au_cpb_removal_delay_minus1" );
    sei.m_auCpbRemovalDelay = code + 1;
    READ_CODE( ( hrd->getDpbOutputDelayLengthMinus1() + 1 ), code, "pic_dpb_output_delay" );
    sei.m_picDpbOutputDelay = code;

    if(hrd->getSubPicCpbParamsPresentFlag())
    {
      READ_CODE(hrd->getDpbOutputDelayDuLengthMinus1()+1, code, "pic_dpb_output_du_delay" );
      sei.m_picDpbOutputDuDelay = code;
    }

    if( hrd->getSubPicCpbParamsPresentFlag() && hrd->getSubPicCpbParamsInPicTimingSEIFlag() )
    {
      READ_UVLC( code, "num_decoding_units_minus1");
      sei.m_numDecodingUnitsMinus1 = code;
      READ_FLAG( code, "du_common_cpb_removal_delay_flag" );
      sei.m_duCommonCpbRemovalDelayFlag = code;
      if( sei.m_duCommonCpbRemovalDelayFlag )
      {
        READ_CODE( ( hrd->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_common_cpb_removal_delay_minus1" );
        sei.m_duCommonCpbRemovalDelayMinus1 = code;
      }
      if( sei.m_numNalusInDuMinus1 != NULL )
      {
        delete sei.m_numNalusInDuMinus1;
      }
      sei.m_numNalusInDuMinus1 = new UInt[ ( sei.m_numDecodingUnitsMinus1 + 1 ) ];
      if( sei.m_duCpbRemovalDelayMinus1  != NULL )
      {
        delete sei.m_duCpbRemovalDelayMinus1;
      }
      sei.m_duCpbRemovalDelayMinus1  = new UInt[ ( sei.m_numDecodingUnitsMinus1 + 1 ) ];

      for( i = 0; i <= sei.m_numDecodingUnitsMinus1; i ++ )
      {
        READ_UVLC( code, "num_nalus_in_du_minus1");
        sei.m_numNalusInDuMinus1[ i ] = code;
        if( ( !sei.m_duCommonCpbRemovalDelayFlag ) && ( i < sei.m_numDecodingUnitsMinus1 ) )
        {
          READ_CODE( ( hrd->getDuCpbRemovalDelayLengthMinus1() + 1 ), code, "du_cpb_removal_delay_minus1" );
          sei.m_duCpbRemovalDelayMinus1[ i ] = code;
        }
      }
    }
  }

  xParseByteAlign();
}


Void SEIReader::xParseSEIRecoveryPoint(SEIRecoveryPoint& sei, UInt /*payloadSize*/)
{
  Int  iCode;
  UInt uiCode;
  READ_SVLC( iCode,  "recovery_poc_cnt" );      sei.m_recoveryPocCnt     = iCode;
  READ_FLAG( uiCode, "exact_matching_flag" );   sei.m_exactMatchingFlag  = uiCode;
  READ_FLAG( uiCode, "broken_link_flag" );      sei.m_brokenLinkFlag     = uiCode;
  xParseByteAlign();
}

Void SEIReader::xParseSEIFramePacking(SEIFramePacking& sei, UInt /*payloadSize*/)
{
  UInt val;
  READ_UVLC( val, "frame_packing_arrangement_id" );                 sei.m_arrangementId = val;
  READ_FLAG( val, "frame_packing_arrangement_cancel_flag" );        sei.m_arrangementCancelFlag = val;

  if ( !sei.m_arrangementCancelFlag )
  {
    READ_CODE( 7, val, "frame_packing_arrangement_type" );          sei.m_arrangementType = val;
    assert((sei.m_arrangementType > 2) && (sei.m_arrangementType < 6) );

    READ_FLAG( val, "quincunx_sampling_flag" );                     sei.m_quincunxSamplingFlag = val;

    READ_CODE( 6, val, "content_interpretation_type" );             sei.m_contentInterpretationType = val;
    READ_FLAG( val, "spatial_flipping_flag" );                      sei.m_spatialFlippingFlag = val;
    READ_FLAG( val, "frame0_flipped_flag" );                        sei.m_frame0FlippedFlag = val;
    READ_FLAG( val, "field_views_flag" );                           sei.m_fieldViewsFlag = val;
    READ_FLAG( val, "current_frame_is_frame0_flag" );               sei.m_currentFrameIsFrame0Flag = val;
    READ_FLAG( val, "frame0_self_contained_flag" );                 sei.m_frame0SelfContainedFlag = val;
    READ_FLAG( val, "frame1_self_contained_flag" );                 sei.m_frame1SelfContainedFlag = val;

    if ( sei.m_quincunxSamplingFlag == 0 && sei.m_arrangementType != 5)
    {
      READ_CODE( 4, val, "frame0_grid_position_x" );                sei.m_frame0GridPositionX = val;
      READ_CODE( 4, val, "frame0_grid_position_y" );                sei.m_frame0GridPositionY = val;
      READ_CODE( 4, val, "frame1_grid_position_x" );                sei.m_frame1GridPositionX = val;
      READ_CODE( 4, val, "frame1_grid_position_y" );                sei.m_frame1GridPositionY = val;
    }

    READ_CODE( 8, val, "frame_packing_arrangement_reserved_byte" );   sei.m_arrangementReservedByte = val;
    READ_FLAG( val,  "frame_packing_arrangement_persistence_flag" );  sei.m_arrangementPersistenceFlag = (val != 0);
  }
  READ_FLAG( val, "upsampled_aspect_ratio" );                       sei.m_upsampledAspectRatio = val;

  xParseByteAlign();
}

Void SEIReader::xParseSEIDisplayOrientation(SEIDisplayOrientation& sei, UInt /*payloadSize*/)
{
  UInt val;
  READ_FLAG( val,       "display_orientation_cancel_flag" );       sei.cancelFlag            = val;
  if( !sei.cancelFlag )
  {
    READ_FLAG( val,     "hor_flip" );                              sei.horFlip               = val;
    READ_FLAG( val,     "ver_flip" );                              sei.verFlip               = val;
    READ_CODE( 16, val, "anticlockwise_rotation" );                sei.anticlockwiseRotation = val;
    READ_FLAG( val,     "display_orientation_persistence_flag" );  sei.persistenceFlag       = val;
  }
  xParseByteAlign();
}

Void SEIReader::xParseSEITemporalLevel0Index(SEITemporalLevel0Index& sei, UInt payloadSize)
{
  UInt val;
  READ_CODE ( 8, val, "tl0_idx" );  sei.tl0Idx = val;
  READ_CODE ( 8, val, "rap_idx" );  sei.rapIdx = val;
  xParseByteAlign();
}

Void SEIReader::xParseSEIGradualDecodingRefreshInfo(SEIGradualDecodingRefreshInfo& sei, UInt /*payloadSize*/)
{
  UInt val;
  READ_FLAG( val, "gdr_foreground_flag" ); sei.m_gdrForegroundFlag = val ? 1 : 0;
  xParseByteAlign();
}

Void SEIReader::xParseSEINoDisplay(SEINoDisplay& sei, UInt /*payloadSize*/)
{
  sei.m_noDisplay = true;
  xParseByteAlign();
}

Void SEIReader::xParseSEIToneMappingInfo(SEIToneMappingInfo& sei, UInt /*payloadSize*/)
{
  Int i;
  UInt val;
  READ_UVLC( val, "tone_map_id" );                         sei.m_toneMapId = val;
  READ_FLAG( val, "tone_map_cancel_flag" );                sei.m_toneMapCancelFlag = val;

  if ( !sei.m_toneMapCancelFlag )
  {
    READ_FLAG( val, "tone_map_persistence_flag" );         sei.m_toneMapPersistenceFlag = val;
    READ_CODE( 8, val, "coded_data_bit_depth" );           sei.m_codedDataBitDepth = val;
    READ_CODE( 8, val, "target_bit_depth" );               sei.m_targetBitDepth = val;
    READ_UVLC( val, "model_id" );                          sei.m_modelId = val;
    switch(sei.m_modelId)
    {
    case 0:
      {
        READ_CODE( 32, val, "min_value" );                 sei.m_minValue = val;
        READ_CODE( 32, val, "max_value" );                 sei.m_maxValue = val;
        break;
      }
    case 1:
      {
        READ_CODE( 32, val, "sigmoid_midpoint" );          sei.m_sigmoidMidpoint = val;
        READ_CODE( 32, val, "sigmoid_width" );             sei.m_sigmoidWidth = val;
        break;
      }
    case 2:
      {
        UInt num = 1u << sei.m_targetBitDepth;
        sei.m_startOfCodedInterval.resize(num+1);
        for(i = 0; i < num; i++)
        {
          READ_CODE( ((( sei.m_codedDataBitDepth + 7 ) >> 3 ) << 3), val, "start_of_coded_interval" );
          sei.m_startOfCodedInterval[i] = val;
        }
        sei.m_startOfCodedInterval[num] = 1u << sei.m_codedDataBitDepth;
        break;
      }
    case 3:
      {
        READ_CODE( 16, val,  "num_pivots" );                       sei.m_numPivots = val;
        sei.m_codedPivotValue.resize(sei.m_numPivots);
        sei.m_targetPivotValue.resize(sei.m_numPivots);
        for(i = 0; i < sei.m_numPivots; i++ )
        {
          READ_CODE( ((( sei.m_codedDataBitDepth + 7 ) >> 3 ) << 3), val, "coded_pivot_value" );
          sei.m_codedPivotValue[i] = val;
          READ_CODE( ((( sei.m_targetBitDepth + 7 ) >> 3 ) << 3),    val, "target_pivot_value" );
          sei.m_targetPivotValue[i] = val;
        }
        break;
      }
    case 4:
      {
        READ_CODE( 8, val, "camera_iso_speed_idc" );                     sei.m_cameraIsoSpeedValue = val;
        if( sei.m_cameraIsoSpeedValue == 255) //Extended_ISO
        {
          READ_CODE( 32,   val,   "camera_iso_speed_value" );            sei.m_cameraIsoSpeedValue = val;
        }
        READ_FLAG( val, "exposure_compensation_value_sign_flag" );       sei.m_exposureCompensationValueSignFlag = val;
        READ_CODE( 16, val, "exposure_compensation_value_numerator" );   sei.m_exposureCompensationValueNumerator = val;
        READ_CODE( 16, val, "exposure_compensation_value_denom_idc" );   sei.m_exposureCompensationValueDenomIdc = val;
        READ_CODE( 32, val, "ref_screen_luminance_white" );              sei.m_refScreenLuminanceWhite = val;
        READ_CODE( 32, val, "extended_range_white_level" );              sei.m_extendedRangeWhiteLevel = val;
        READ_CODE( 16, val, "nominal_black_level_luma_code_value" );     sei.m_nominalBlackLevelLumaCodeValue = val;
        READ_CODE( 16, val, "nominal_white_level_luma_code_value" );     sei.m_nominalWhiteLevelLumaCodeValue= val;
        READ_CODE( 16, val, "extended_white_level_luma_code_value" );    sei.m_extendedWhiteLevelLumaCodeValue = val;
        break;
      }
    default:
      {
        assert(!"Undefined SEIToneMapModelId");
        break;
      }
    }//switch model id
  }// if(!sei.m_toneMapCancelFlag)

  xParseByteAlign();
}

Void SEIReader::xParseSEISOPDescription(SEISOPDescription &sei, UInt payloadSize)
{
  Int iCode;
  UInt uiCode;

  READ_UVLC( uiCode,           "sop_seq_parameter_set_id"            ); sei.m_sopSeqParameterSetId = uiCode;
  READ_UVLC( uiCode,           "num_pics_in_sop_minus1"              ); sei.m_numPicsInSopMinus1 = uiCode;
  for (UInt i = 0; i <= sei.m_numPicsInSopMinus1; i++)
  {
    READ_CODE( 6, uiCode,                     "sop_desc_vcl_nalu_type" );  sei.m_sopDescVclNaluType[i] = uiCode;
    READ_CODE( 3, sei.m_sopDescTemporalId[i], "sop_desc_temporal_id"   );  sei.m_sopDescTemporalId[i] = uiCode;
    if (sei.m_sopDescVclNaluType[i] != NAL_UNIT_CODED_SLICE_IDR_W_RADL && sei.m_sopDescVclNaluType[i] != NAL_UNIT_CODED_SLICE_IDR_N_LP)
    {
      READ_UVLC( sei.m_sopDescStRpsIdx[i],    "sop_desc_st_rps_idx"    ); sei.m_sopDescStRpsIdx[i] = uiCode;
    }
    if (i > 0)
    {
      READ_SVLC( iCode,                       "sop_desc_poc_delta"     ); sei.m_sopDescPocDelta[i] = iCode;
    }
  }

  xParseByteAlign();
}

Void SEIReader::xParseSEIScalableNesting(SEIScalableNesting& sei, const NalUnitType nalUnitType, UInt payloadSize, TComSPS *sps)
{
  UInt uiCode;
  SEIMessages seis;

  READ_FLAG( uiCode,            "bitstream_subset_flag"         ); sei.m_bitStreamSubsetFlag = uiCode;
  READ_FLAG( uiCode,            "nesting_op_flag"               ); sei.m_nestingOpFlag = uiCode;
  if (sei.m_nestingOpFlag)
  {
    READ_FLAG( uiCode,            "default_op_flag"               ); sei.m_defaultOpFlag = uiCode;
    READ_UVLC( uiCode,            "nesting_num_ops_minus1"        ); sei.m_nestingNumOpsMinus1 = uiCode;
    for (UInt i = sei.m_defaultOpFlag; i <= sei.m_nestingNumOpsMinus1; i++)
    {
      READ_CODE( 3,        uiCode,  "nesting_max_temporal_id_plus1"   ); sei.m_nestingMaxTemporalIdPlus1[i] = uiCode;
      READ_UVLC( uiCode,            "nesting_op_idx"                  ); sei.m_nestingOpIdx[i] = uiCode;
    }
  }
  else
  {
    READ_FLAG( uiCode,            "all_layers_flag"               ); sei.m_allLayersFlag       = uiCode;
    if (!sei.m_allLayersFlag)
    {
      READ_CODE( 3,        uiCode,  "nesting_no_op_max_temporal_id_plus1"  ); sei.m_nestingNoOpMaxTemporalIdPlus1 = uiCode;
      READ_UVLC( uiCode,            "nesting_num_layers_minus1"            ); sei.m_nestingNumLayersMinus1        = uiCode;
      for (UInt i = 0; i <= sei.m_nestingNumLayersMinus1; i++)
      {
        READ_CODE( 6,           uiCode,     "nesting_layer_id"      ); sei.m_nestingLayerId[i]   = uiCode;
      }
    }
  }

  // byte alignment
  while ( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    UInt code;
    READ_FLAG( code, "nesting_zero_bit" );
  }

  sei.m_callerOwnsSEIs = false;

  // read nested SEI messages
  do {
    xReadSEImessage(sei.m_nestedSEIs, nalUnitType, sps);
  } while (m_pcBitstream->getNumBitsLeft() > 8);

}

#if RExt__O0099_TIME_CODE_SEI
Void SEIReader::xParseSEITimeCode(SEITimeCode& sei, UInt payloadSize)
{
  UInt code;
  READ_CODE(2, code, "num_clock_ts"); sei.numClockTs = code;
  for(int i = 0; i < sei.numClockTs; i++)
  {
    READ_FLAG(code, "clock_time_stamp_flag"); sei.clockTimeStampFlag[i] = code;
    if(sei.clockTimeStampFlag[i])
    {
      READ_FLAG(code, "nuit_field_based_flag"); sei.nuitFieldBasedFlag = code;
      READ_CODE(5, code, "counting_type"); sei.countingType = code;
      READ_FLAG(code, "full_timestamp_flag"); sei.fullTimeStampFlag = code;
      READ_FLAG(code, "discontinuity_flag"); sei.discontinuityFlag = code;
      READ_FLAG(code, "cnt_dropped_flag"); sei.cntDroppedFlag = code;
      READ_CODE(9, code, "n_frames"); sei.nFrames = code;
      if(sei.fullTimeStampFlag)
      {
        READ_CODE(6, code, "seconds_value"); sei.secondsValue = code;
        READ_CODE(6, code, "minutes_value"); sei.minutesValue = code;
        READ_CODE(5, code, "hours_value"); sei.hoursValue = code;
      }
      else
      {
        READ_FLAG(code, "seconds_flag"); sei.secondsFlag = code;
        if(sei.secondsFlag)
        {
          READ_CODE(6, code, "seconds_value"); sei.secondsValue = code;
          READ_FLAG(code, "minutes_flag"); sei.minutesFlag = code;
          if(sei.minutesFlag)
          {
            READ_CODE(6, code, "minutes_value"); sei.minutesValue = code;
            READ_FLAG(code, "hours_flag"); sei.hoursFlag = code;
            if(sei.hoursFlag)
              READ_CODE(5, code, "hours_value"); sei.hoursValue = code;
          }
        }
      }
      READ_CODE(5, code, "time_offset_length"); sei.timeOffsetLength = code;
      if(sei.timeOffsetLength > 0)
      {
        READ_CODE(sei.timeOffsetLength, code, "time_offset");
        if((code & (1 << (sei.timeOffsetLength-1))) == 0)
        {
          sei.timeOffset = code;
        }
        else
        {
          code &= (1<< (sei.timeOffsetLength-1)) - 1;
          sei.timeOffset = ~code + 1;
        }
      }
    }
  }
  xParseByteAlign();
}
#endif

#if RExt__O0079_CHROMA_SAMPLING_FILTER_HINT_SEI
Void SEIReader::xParseSEIChromaSamplingFilterHint(SEIChromaSamplingFilterHint& sei, UInt /*payloadSize*//*, TComSPS* sps*/)
{
  UInt uiCode;

  //NOTE: RExt - Made unconditional to be consistent with the working text P1005
  //if(sps->getVuiParameters()->getChromaLocInfoPresentFlag())
  //{
  READ_CODE(8, uiCode, "ver_chroma_filter_idc"); sei.m_verChromaFilterIdc = uiCode;
  READ_CODE(8, uiCode, "hor_chroma_filter_idc"); sei.m_horChromaFilterIdc = uiCode;
  READ_FLAG(uiCode, "ver_filtering_process_flag"); sei.m_verFilteringProcessFlag = uiCode;
  if(sei.m_verChromaFilterIdc == 1 || sei.m_horChromaFilterIdc == 1)
  {
    READ_UVLC(uiCode, "target_format_idc"); sei.m_targetFormatIdc = uiCode;
    READ_FLAG(uiCode, "perfect_reconstruction_flag"); sei.m_perfectReconstructionFlag = uiCode;
    if(sei.m_verChromaFilterIdc == 1)
    {
      READ_UVLC(uiCode, "num_vertical_filters"); sei.m_numVerticalFilters = uiCode;
      if(sei.m_numVerticalFilters > 0)
      {
        sei.m_verTapLengthMinus1 = (Int*)malloc(sei.m_numVerticalFilters * sizeof(Int));
        sei.m_verFilterCoeff = (Int**)malloc(sei.m_numVerticalFilters * sizeof(Int*));
        for(Int i = 0; i < sei.m_numVerticalFilters; i ++)
        {
          READ_UVLC(uiCode, "ver_tap_length_minus_1"); sei.m_verTapLengthMinus1[i] = uiCode; 
          sei.m_verFilterCoeff[i] = (Int*)malloc(sei.m_verTapLengthMinus1[i] * sizeof(Int));
          for(Int j = 0; j < sei.m_verTapLengthMinus1[i]; j ++)
          {
            READ_SVLC(sei.m_verFilterCoeff[i][j], "ver_filter_coeff");
          }
        }
      }
    }
    if(sei.m_horChromaFilterIdc == 1)
    {
      READ_UVLC(uiCode, "num_horizontal_filters"); sei.m_numHorizontalFilters = uiCode;
      if(sei.m_numHorizontalFilters  > 0)
      {
        sei.m_horTapLengthMinus1 = (Int*)malloc(sei.m_numHorizontalFilters * sizeof(Int));
        sei.m_horFilterCoeff = (Int**)malloc(sei.m_numHorizontalFilters * sizeof(Int*));
        for(Int i = 0; i < sei.m_numHorizontalFilters; i ++)
        {
          READ_UVLC(uiCode, "hor_tap_length_minus_1"); sei.m_horTapLengthMinus1[i] = uiCode;
          sei.m_horFilterCoeff[i] = (Int*)malloc(sei.m_horTapLengthMinus1[i] * sizeof(Int));
          for(Int j = 0; j < sei.m_horTapLengthMinus1[i]; j ++)
          {
            READ_SVLC(sei.m_horFilterCoeff[i][j], "hor_filter_coeff");
          }
        }
      }
    }
  }
  //}

  //// DEBUG
  //printf("summary of chroma sampling filter sei message:\n");
  //if(sps->getVuiParameters()->getChromaLocInfoPresentFlag())
  //{
  //  printf("ver_chroma_filter_idc = %d\n", sei.m_verChromaFilterIdc);
  //  printf("hor_chroma_filter_idc = %d\n", sei.m_horChromaFilterIdc);
  //  printf("ver_filtering_process_flag = %d\n", sei.m_verFilteringProcessFlag);
  //  if(sei.m_verChromaFilterIdc == 1 || sei.m_horChromaFilterIdc == 1)
  //  {
  //    printf("target_format_idc = %d\n", sei.m_targetFormatIdc);
  //    printf("perfect_reconstruction_flag = %d\n", sei.m_perfectReconstructionFlag);
  //    if(sei.m_verChromaFilterIdc == 1)
  //    {
  //      printf("num_vertical_filters = %d\n", sei.m_numVerticalFilters);
  //      for(Int i = 0; i < sei.m_numVerticalFilters; i ++)
  //      {
  //        printf("ver_tap_length_minus_1[%d] = %d\n", i, sei.m_verTapLengthMinus1[i]);
  //        for(Int j = 0; j < sei.m_verTapLengthMinus1[i]; j ++)
  //        {
  //          printf("ver_filter_coeff[%d][%d] = %d\n", i, j, sei.m_verFilterCoeff[i][j]);
  //        }
  //      }
  //    }
  //    if(sei.m_horChromaFilterIdc == 1)
  //    {
  //      printf("num_horizontal_filters = %d\n", sei.m_numHorizontalFilters);
  //      if(sei.m_numHorizontalFilters  > 0)
  //      {
  //        for(Int i = 0; i < sei.m_numHorizontalFilters; i ++)
  //        {
  //          printf("hor_tap_length_minus_1[%d] = %d\n", i, sei.m_horTapLengthMinus1[i]);
  //          for(Int j = 0; j < sei.m_horTapLengthMinus1[i]; j ++)
  //          {
  //            printf("hor_filter_coeff[%d][%d] = %d\n", i, j, sei.m_horFilterCoeff[i][j]);
  //          }
  //        }
  //      }
  //    }
  //  }
  //}
  //else
  //{
  //  printf("ChromaLocInfoPresentFlag = false\nNo message read\n");
  //}
}
#endif

Void SEIReader::xParseSEIKneeFunctionInfo(SEIKneeFunctionInfo& sei, UInt payloadSize){
  Int i;
  UInt val;
  READ_UVLC( val, "knee_function_id" );                   sei.m_kneeId = val;
  READ_FLAG( val, "knee_function_cancel_flag" );          sei.m_kneeCancelFlag = val;
  if ( !sei.m_kneeCancelFlag )
  {
    READ_FLAG( val, "knee_function_persistence_flag" );   sei.m_kneePersistenceFlag = val;
    READ_FLAG( val, "mapping_flag" );                     sei.m_kneeMappingFlag = val;
    READ_CODE( 32, val, "input_d_range" );                sei.m_kneeInputDrange = val;
    READ_CODE( 32, val, "input_disp_luminance" );         sei.m_kneeInputDispLuminance = val;
    READ_CODE( 32, val, "output_d_range" );               sei.m_kneeOutputDrange = val;
    READ_CODE( 32, val, "output_disp_luminance" );        sei.m_kneeOutputDispLuminance = val;
    READ_UVLC( val, "num_knee_points_minus1" );           sei.m_kneeNumKneePointsMinus1 = val;
    assert( sei.m_kneeNumKneePointsMinus1 > 0 );
    sei.m_kneeInputKneePoint.resize(sei.m_kneeNumKneePointsMinus1+1);
    sei.m_kneeOutputKneePoint.resize(sei.m_kneeNumKneePointsMinus1+1);
    for(i = 0; i <= sei.m_kneeNumKneePointsMinus1; i++ )
    {
      READ_CODE( 10, val, "input_knee_point" );           sei.m_kneeInputKneePoint[i] = val;
      READ_CODE( 10, val, "output_knee_point" );          sei.m_kneeOutputKneePoint[i] = val;
    }
  }
}

#if RExt__P0084_MASTERING_DISPLAY_COLOUR_VOLUME_SEI
Void SEIReader::xParseSEIMasteringDisplayColourVolume(SEIMasteringDisplayColourVolume& sei, UInt payloadSize)
{
  UInt code;
 
  READ_CODE( 16, code, "display_primaries_x[0]" ); sei.displayPrimaries[0][0] = code;
  READ_CODE( 16, code, "display_primaries_y[0]" ); sei.displayPrimaries[0][1] = code;
    
  READ_CODE( 16, code, "display_primaries_x[1]" ); sei.displayPrimaries[1][0] = code;
  READ_CODE( 16, code, "display_primaries_y[1]" ); sei.displayPrimaries[1][1] = code;
    
  READ_CODE( 16, code, "display_primaries_x[2]" ); sei.displayPrimaries[2][0] = code;
  READ_CODE( 16, code, "display_primaries_y[2]" ); sei.displayPrimaries[2][1] = code;
    
    
  READ_CODE( 16, code, "white_point_x" ); sei.displayWhitePoint[0] = code;
  READ_CODE( 16, code, "white_point_y" ); sei.displayWhitePoint[1] = code;
    
  READ_CODE( 32, code, "max_display_mastering_luminance" ); sei.maxDisplayLuminance = code;
  READ_CODE( 32, code, "min_display_mastering_luminance" ); sei.minDisplayLuminance = code;
 
  xParseByteAlign();
}
#endif

Void SEIReader::xParseByteAlign()
{
  UInt code;
  if( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    READ_FLAG( code, "bit_equal_to_one" );          assert( code == 1 );
  }
  while( m_pcBitstream->getNumBitsRead() % 8 != 0 )
  {
    READ_FLAG( code, "bit_equal_to_zero" );         assert( code == 0 );
  }
}

//! \}
