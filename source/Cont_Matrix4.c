/**********************************************************************
  Cont_Matrix4.c:

     Cont_Matrix4.c is a subroutine to contract a Matrix4 (HVNA3).

  Log of Cont_Matrix4.c:

     23/Feb./2012  Released by T.Ozaki

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "openmx_common.h"

void Cont_Matrix4(double ****Mat, double ****CMat)
{
  int spin,Mc_AN,Gc_AN,h_AN,L,L1,M1;
  int p,q,p0,q0,al,be,Hwan,Gh_AN,Mh_AN;
  double sumS,tmp0;

  for (Mc_AN=1; Mc_AN<=Matomnum; Mc_AN++){

    Gc_AN = M2G[Mc_AN];

    for (h_AN=0; h_AN<=FNAN[Gc_AN]; h_AN++){

      Gh_AN = natn[Gc_AN][h_AN];        
      Hwan = WhatSpecies[Gh_AN];
      Mh_AN = F_G2M[Gh_AN];

      for (al=0; al<Spe_Total_CNO[Hwan]; al++){
	for (be=0; be<Spe_Total_CNO[Hwan]; be++){
	  sumS = 0.0;
	  for (p=0; p<Spe_Specified_Num[Hwan][al]; p++){
	    p0 = Spe_Trans_Orbital[Hwan][al][p];
	    for (q=0; q<Spe_Specified_Num[Hwan][be]; q++){
	      q0 = Spe_Trans_Orbital[Hwan][be][q];
	      tmp0 = CntCoes[Mh_AN][al][p]*CntCoes[Mh_AN][be][q]; 
	      sumS += tmp0*Mat[Mc_AN][h_AN][p0][q0];
	    }
	  }
	  CMat[Mc_AN][h_AN][al][be] = sumS;
	}
      }
    }
  }

}
