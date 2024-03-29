/*
 * Copyright (c) 2003 Matteo Frigo
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* This file was automatically generated --- DO NOT EDIT */
/* Generated on Sat Jul  5 21:58:03 EDT 2003 */

#include "codelet-rdft.h"

/* Generated by: /homee/stevenj/cvs/fftw3.0.1/genfft/gen_r2hc -compact -variables 4 -n 6 -name r2hcII_6 -dft-II -include r2hcII.h */

/*
 * This function contains 13 FP additions, 4 FP multiplications,
 * (or, 11 additions, 2 multiplications, 2 fused multiply/add),
 * 14 stack variables, and 12 memory accesses
 */
/*
 * Generator Id's : 
 * $Id: algsimp.ml,v 1.7 2003/03/15 20:29:42 stevenj Exp $
 * $Id: fft.ml,v 1.2 2003/03/15 20:29:42 stevenj Exp $
 * $Id: gen_r2hc.ml,v 1.13 2003/04/17 19:25:50 athena Exp $
 */

#include "r2hcII.h"

static void r2hcII_6(const R *I, R *ro, R *io, stride is, stride ros, stride ios, int v, int ivs, int ovs)
{
     DK(KP500000000, +0.500000000000000000000000000000000000000000000);
     DK(KP866025403, +0.866025403784438646763723170752936183471402627);
     int i;
     for (i = v; i > 0; i = i - 1, I = I + ivs, ro = ro + ovs, io = io + ovs) {
	  E Ta, T7, T9, T1, T3, T2, T8, T4, T5, T6, Tb;
	  Ta = I[WS(is, 3)];
	  T5 = I[WS(is, 5)];
	  T6 = I[WS(is, 1)];
	  T7 = KP866025403 * (T5 - T6);
	  T9 = T5 + T6;
	  T1 = I[0];
	  T3 = I[WS(is, 2)];
	  T2 = I[WS(is, 4)];
	  T8 = KP866025403 * (T2 + T3);
	  T4 = FMA(KP500000000, T3 - T2, T1);
	  ro[0] = T4 - T7;
	  ro[WS(ros, 2)] = T4 + T7;
	  io[WS(ios, 1)] = Ta - T9;
	  ro[WS(ros, 1)] = T1 + T2 - T3;
	  Tb = FMA(KP500000000, T9, Ta);
	  io[0] = -(T8 + Tb);
	  io[WS(ios, 2)] = T8 - Tb;
     }
}

static const kr2hc_desc desc = { 6, "r2hcII_6", {11, 2, 2, 0}, &GENUS, 0, 0, 0, 0, 0 };

void X(codelet_r2hcII_6) (planner *p) {
     X(kr2hcII_register) (p, r2hcII_6, &desc);
}
