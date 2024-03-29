/**
 *
 * Lame ACM wrapper, encode/decode MP3 based RIFF/AVI files in MS Windows
 *
 *  Copyright (c) 2002 Steve Lhomme <steve.lhomme at free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
/*!
	\author Steve Lhomme
	\version \$Id: DecodeStream.h,v 1.1.1.1 2003/10/14 07:54:36 heroine Exp $
*/

#if !defined(_DECODESTREAM_H__INCLUDED_)
#define _DECODESTREAM_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>

#include "ADbg/ADbg.h"

#include <config.h>
#include "util.h"
#include "interface.h"

class DecodeStream
{
public:
	DecodeStream( );
	virtual ~DecodeStream( );

	static DecodeStream * Create();
	static const bool Erase(const DecodeStream * a_ACMStream);

	bool init(const int nSamplesPerSec, const int nChannels, const int nAvgBytesPerSec, const int nSourceBitrate);
	bool open();
	bool close(LPBYTE pOutputBuffer, DWORD *pOutputSize);

	DWORD GetOutputSizeForInput(const DWORD the_SrcLength) const;
	bool  ConvertBuffer(LPACMDRVSTREAMHEADER a_StreamHeader);

	static unsigned int GetOutputSampleRate(int samples_per_sec, int bitrate, int channels);

protected:
	lame_global_flags * gfp;

	ADbg * my_debug;
	int my_SamplesPerSec;
	int my_Channels;
	int my_AvgBytesPerSec;
	DWORD  my_SamplesPerBlock;
	int my_SourceBitrate;

	MPSTR my_DecodeData;

	unsigned int m_WorkingBufferUseSize;
	char m_WorkingBuffer[2304*2]; // should be at least twice my_SamplesPerBlock

	inline int GetBytesPerBlock(DWORD bytes_per_sec, DWORD samples_per_sec, int BlockAlign) const;

};

#endif // !defined(_DECODESTREAM_H__INCLUDED_)

