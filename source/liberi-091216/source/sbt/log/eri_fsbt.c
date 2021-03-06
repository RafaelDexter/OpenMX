/*----------------------------------------------------------------------
  eri_fsbt.c

  Routine to perform the fast spherical Bessel transform (FSBT).
  This also manages the logarithmic radial mesh information.
 
  Reference:

  [1] A. E. Siegman, 
      "Quasi fast Hankel transform," 
      Opt. Lett. vol. 1, pp. 13 (1977). 

  [2] J. D. Talman, 
      "Numerical Fourier and Bessel Transforms in Logarithmic Variables,"
      J. Comp. Phys. vol. 29, pp. 35 (1978).

  [3] J. D. Talman, 
      "Numerical Methods for Multicenter Integrals for Numerically
       Defined Basis Functions Applied in Molecular Calculations," 
      Int. J. Quantum. Chem. vol. 93, pp. 72 (2003).

  Jul. 2008, M. Toyoda
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <fftw3.h>
#include "eri_def.h"
#include "eri_fsbt.h"


static fftw_complex g_tmp_in[ERI_NGRIDMAX];
static fftw_complex g_tmp_out[ERI_NGRIDMAX];

struct ERI_FSBT_Struct {
  int          ngrid;
  int          lmax;
  double       drho;
  double       dt;

  double       *t;
  double       *rho;
  double       *r;
  fftw_complex *M;

  fftw_complex *phi;
  int           phi_m;
};




/*----------------------------------------------------------------------
  M function

  See eq. (17) in ref. [2], or eq. (3.12) in ref. [3].
----------------------------------------------------------------------*/
static void M_func(
  double        t, /* (IN)  argument */
  int           l, /* (IN)  nonnegative integer */
  int           m, /* (IN)  integer m = 0, 1, ... , l */
  fftw_complex *M  /* (OUT) function value */
)
{
  const int n = 10;
  int j, p;
  double p1, p2, cp, sp, cr, ci, re, im, norm, nh, rr, r, phi, e;
  fftw_complex prod;

  assert( m <= l );

  p  = l-m;
  cp = cos(PI*(double)p/2.0);
  sp = sin(PI*(double)p/2.0);

  /* phi_1i : See eq. (9) in [2], or eq. (3.13) in [3] */
  nh  = 0.5+(double)n;
  rr  = nh*nh + t*t;
  r   = sqrt(rr);
  phi = atan2(2.0*t, 2.0*nh); 
  p1 = t*(1.0-log(r)) - phi*(double)n 
       + (sin(phi)-(sin(3.0*phi)-sin(5.0*phi)/3.5/rr)/30.0/rr)/12.0/r;
  for (j=0; j<n; j++) { 
    p1 += atan2(2.0*t, 1.0+2.0*(double)j); 
  }

  /* phi_2 : See eq. (8) in [2], or eq. (3.14) in [3] */
  e  = exp(PI*t);
  p2 = atan2(e-1.0, e+1.0);

  prod[0] = 1.0/sqrt(8.0*PI);   
  prod[1] = 0.0;

  ci = -t;
  for (j=0; j<p; j++) {
    cr = 0.5+(double)j;
    re = prod[0]*cr - prod[1]*ci;
    im = prod[0]*ci + prod[1]*cr;
    prod[0] = re;
    prod[1] = im;
  }

  ci = t;
  for (j=0; j<l; j++) {
    cr = 1.5+(double)(2*j)-(double)p;
    norm = cr*cr + ci*ci;
    re = (prod[0]*cr + prod[1]*ci)/norm;
    im = (prod[1]*cr - prod[0]*ci)/norm;
    prod[0] = re;
    prod[1] = im;
  }
  
  cr = cp*cos(p1-p2)+sp*cos(p1+p2);
  ci = cp*sin(p1-p2)+sp*sin(p1+p2);
  (*M)[0] = prod[0]*cr - prod[1]*ci;
  (*M)[1] = prod[0]*ci + prod[1]*cr;
}





/*----------------------------------------------------------------------
  ERI_FastSBT_Init

  Initialier function.
  Workspace memory is reserved for grids, M functions and stuff.
  This also generates the logarithmic radial mesh grid (rho and kap for
  real and reciprocal spaces, respectively), which is defined as:
  
    r = exp(rho)
    k = exp(kap)

  The mesh points for rho and kap are linear.
  The interval is given by the interval for t-mesh:
    drho  = 2.0*PI/dt/nmesh
  
  This must be called before any call of FastSBT_Transform.
----------------------------------------------------------------------*/
ERI_FSBT_t* ERI_FSBT_Init(
  int    lmax,  /* (IN) maximum number of angular momentum */
  int    ngrid, /* (IN) number of radial logarithm mesh */
  double rho0,  /* (IN) lower bound of rho (radial) mesh */
  double dt     /* (IN) interval of t-mesh */
)
{
  int i, j, l, m;
  double t0, rho, drho;
  ERI_FSBT_t *ptr;
 
  STEPTRACE( "FSBT_Init: in" );

  ptr = (ERI_FSBT_t*)malloc(sizeof(ERI_FSBT_t));
  if (NULL==ptr) { return NULL; }

  /* allocate arrays */
  ptr->t   = (double*)malloc(sizeof(double)*ngrid); 
  ptr->rho = (double*)malloc(sizeof(double)*ngrid);
  ptr->r   = (double*)malloc(sizeof(double)*ngrid);
  ptr->M   = (fftw_complex*) fftw_malloc(
               sizeof(fftw_complex)*lmax*(lmax+1)*ngrid);
  ptr->phi = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*ngrid);

  if (NULL==ptr->t || NULL==ptr->rho || NULL==ptr->r 
    || NULL==ptr->M ) {
    ERI_FSBT_Free(ptr);
    return NULL;
  }

  /* t-mesh */ 
  t0 = -0.5*dt*(double)ngrid; 
  for (i=0; i<ngrid; i++) { ptr->t[i] = t0+dt*(double)i; }

  /* M function */
  for (l=0; l<lmax; l++) {
    for (m=0; m<=l; m++) {
      j = l*(l+1)+m;
      for (i=0; i<ngrid; i++) {
        M_func(ptr->t[i], l, m, &ptr->M[j*ngrid+i]);
      }
    } 
  }
 
  /* rho- and r-mesh */ 
  drho = 2.0*PI/(double)ngrid/dt;
  for (i=0; i<ngrid; i++) {
    rho = rho0 + drho*(double)i;
    ptr->rho[i] = rho;
    ptr->r[i]   = exp(rho);
  }

  /* save parameters */ 
  ptr->lmax  = lmax;
  ptr->ngrid = ngrid;
  ptr->drho  = drho;
  ptr->dt    = dt;
  
  STEPTRACE( "ERI_FSBT_Init: out" );

  return ptr;
}




size_t ERI_FSBT_Required_Size(
  int lmax, /* (IN) maximum number of angular momentum */
  int ngrid /* (IN) number of radial logarithm mesh */
)
{
  return sizeof(ERI_FSBT_t)
       + sizeof(double)*ngrid /* t */
       + sizeof(double)*ngrid /* rho */
       + sizeof(double)*ngrid /* r */
       + sizeof(fftw_complex)*lmax*(lmax+1)*ngrid /* M */
       + sizeof(fftw_complex)*ngrid /* phi */
  ;
}




void ERI_FSBT_Free(ERI_FSBT_t *ptr)
{
  if (ptr) {
    if (ptr->t)   { free(ptr->t);    }
    if (ptr->rho) { free(ptr->rho);  } 
    if (ptr->r)   { free(ptr->r);    }
    if (ptr->M)   { fftw_free(ptr->M);   }
    if (ptr->phi) { fftw_free(ptr->phi); }
    free(ptr);
  }
}





void ERI_FSBT_Transform_Input(
  ERI_FSBT_t   *ptr,
  const double *in,  /* (IN)  input function */
  int           m    /* (IN)  parameters of transform */
) 
{
  int i;
  double rho, p, c, s, e;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_phi = ptr->phi;

  for (i=0; i<ngrid; i++) { 
    rho = rhomesh[i];
    p   = t0*rho;
    c   = cos(p);
    s   = sin(p);
    e   = exp((1.5+(double)m)*rho);
    tmp_in[i][0] = e*(in[2*i+0]*c-in[2*i+1]*s);
    tmp_in[i][1] = e*(in[2*i+0]*s+in[2*i+1]*c);
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_phi, 
                          FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);

  ptr->phi_m = m;
}




void ERI_FSBT_Transform_Output(
  ERI_FSBT_t *ptr,
  double     *out, /* (OUT) output function */
  int         l    /* (IN)  parameters of transform */
) 
{
  int i, im;
  double rho, kap, t, p, c, s, e;
  fftw_complex prod;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        rho0    = ptr->rho[0];
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;
  const double *tmesh   = ptr->t;
  int           phi_m   = ptr->phi_m;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_phi = ptr->phi;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_out = g_tmp_out;

  double        drho    = ptr->drho;
  double        dt      = ptr->dt;
 
  for (i=0; i<ngrid; i++) { 
    t = tmesh[i];
    p = rho0*(t-t0)+rho0*t; 
    c = cos(p);
    s = sin(p);
    im = (l*(l+1)+phi_m)*ngrid+i;
    prod[0] = tmp_phi[i][0]*Mmesh[im][0] - tmp_phi[i][1]*Mmesh[im][1];
    prod[1] = tmp_phi[i][0]*Mmesh[im][1] + tmp_phi[i][1]*Mmesh[im][0];
    tmp_in[i][0] = drho*(prod[0]*c - prod[1]*s);
    tmp_in[i][1] = drho*(prod[0]*s + prod[1]*c);
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_out, 
                          FFTW_BACKWARD, FFTW_ESTIMATE); 
  fftw_execute(plan);
  fftw_destroy_plan(plan);
  
  for (i=0; i<ngrid; i++) { 
    kap = rhomesh[i];
    p   = t0*(kap-rho0); 
    c   = cos(p);
    s   = sin(p);
    e   = exp(((double)phi_m-1.5)*kap)*dt;
    out[2*i+0] = e*(tmp_out[i][0]*c - tmp_out[i][1]*s);
    out[2*i+1] = e*(tmp_out[i][0]*s + tmp_out[i][1]*c);
  }
}




void ERI_FSBT_Transform(
  ERI_FSBT_t   *ptr,
  double       *out, /* (OUT) transformed function */
  const double *in,  /* (IN)  input function */
  int           l,   /* (IN)  order of transform */
  int           m    /* (IN)  parameters of transform */
) 
{
  int i, im;
  double rho, kap, t;
  double p, c, s, e;
  fftw_complex prod;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        rho0    = ptr->rho[0];
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;
  const double *tmesh   = ptr->t;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_out = g_tmp_out;

  double        drho    = ptr->drho;
  double        dt      = ptr->dt;
 
  /* the first transform (phi is calculated) */ 
  for (i=0; i<ngrid; i++) { 
    rho = rhomesh[i];
    p   = t0*rho;
    c   = cos(p);
    s   = sin(p);
    e   = exp((1.5+(double)m)*rho);
    tmp_in[i][0] = e*(in[2*i+0]*c-in[2*i+1]*s);
    tmp_in[i][1] = e*(in[2*i+0]*s+in[2*i+1]*c);
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_out, 
                          FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
 
  /* the second transform */ 
  for (i=0; i<ngrid; i++) { 
    t = tmesh[i];
    p = rho0*(t-t0)+rho0*t; 
    c = cos(p);
    s = sin(p);
    im = (l*(l+1)+m)*ngrid+i;
    prod[0] = tmp_out[i][0]*Mmesh[im][0] - tmp_out[i][1]*Mmesh[im][1];
    prod[1] = tmp_out[i][0]*Mmesh[im][1] + tmp_out[i][1]*Mmesh[im][0];
    tmp_in[i][0] = drho*(prod[0]*c - prod[1]*s);
    tmp_in[i][1] = drho*(prod[0]*s + prod[1]*c);
  }
  fftw_execute(plan);
  fftw_destroy_plan(plan);
  
  for (i=0; i<ngrid; i++) { 
    kap = rhomesh[i];
    p   = t0*(kap-rho0); 
    c   = cos(p);
    s   = sin(p);
    e   = exp(((double)m-1.5)*kap)*dt;
    out[2*i+0] = e*(tmp_out[i][0]*c - tmp_out[i][1]*s);
    out[2*i+1] = e*(tmp_out[i][0]*s + tmp_out[i][1]*c);
  }
}



#if 0
void ERI_FSBT_Transform_Input_Real(
  ERI_FSBT_t   *ptr,
  const double *in,  /* (IN)  input function */
  int           m    /* (IN)  parameters of transform */
) 
{
  int i;
  double rho, p, c, s, e;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_phi = ptr->phi;

  for (i=0; i<ngrid; i++) { 
    rho = rhomesh[i];
    p   = t0*rho;
    c   = cos(p);
    s   = sin(p);
    e   = exp((1.5+(double)m)*rho);
    tmp_in[i][0] = e*in[i]*c;
    tmp_in[i][1] = e*in[i]*s;
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_phi, 
                          FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);

  ptr->phi_m = m;
}




void ERI_FSBT_Transform_Output_Real(
  ERI_FSBT_t *ptr,
  double     *out, /* (OUT) output function */
  int         l    /* (IN)  parameters of transform */
) 
{
  int i, im;
  double rho, kap, t, p, c, s, e;
  fftw_complex prod;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        rho0    = ptr->rho[0];
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;
  const double *tmesh   = ptr->t;
  int           phi_m   = ptr->phi_m;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_phi = ptr->phi;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_out = g_tmp_out;

  double        drho    = ptr->drho;
  double        dt      = ptr->dt;
 
  for (i=0; i<ngrid; i++) { 
    t = tmesh[i];
    p = rho0*(t-t0)+rho0*t; 
    c = cos(p);
    s = sin(p);
    im = (l*(l+1)+phi_m)*ngrid+i;
    prod[0] = tmp_phi[i][0]*Mmesh[im][0] - tmp_phi[i][1]*Mmesh[im][1];
    prod[1] = tmp_phi[i][0]*Mmesh[im][1] + tmp_phi[i][1]*Mmesh[im][0];
    tmp_in[i][0] = drho*(prod[0]*c - prod[1]*s);
    tmp_in[i][1] = drho*(prod[0]*s + prod[1]*c);
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_out, 
                          FFTW_BACKWARD, FFTW_ESTIMATE); 
  fftw_execute(plan);
  fftw_destroy_plan(plan);
  
  for (i=0; i<ngrid; i++) { 
    kap = rhomesh[i];
    p   = t0*(kap-rho0); 
    c   = cos(p);
    s   = sin(p);
    e   = exp(((double)phi_m-1.5)*kap)*dt;
    out[i] = e*(tmp_out[i][0]*c - tmp_out[i][1]*s);
    //out[2*i+1] = e*(tmp_out[i][0]*s + tmp_out[i][1]*c);
  }
}




void ERI_FSBT_Transform_Real(
  ERI_FSBT_t   *ptr,
  double       *out, /* (OUT) transformed function */
  const double *in,  /* (IN)  input function */
  int           l,   /* (IN)  order of transform */
  int           m    /* (IN)  parameters of transform */
) 
{
  int i, im;
  double rho, kap, t;
  double p, c, s, e;
  fftw_complex prod;
  fftw_plan plan;

  /* mesh information */
  int           ngrid   = ptr->ngrid;
  double        rho0    = ptr->rho[0];
  double        t0      = ptr->t[0];
  const double *rhomesh = ptr->rho;
  const double *tmesh   = ptr->t;

  fftw_complex *Mmesh   = ptr->M;
  fftw_complex *tmp_in  = g_tmp_in;
  fftw_complex *tmp_out = g_tmp_out;

  double        drho    = ptr->drho;
  double        dt      = ptr->dt;
 
  /* the first transform (phi is calculated) */ 
  for (i=0; i<ngrid; i++) { 
    rho = rhomesh[i];
    p   = t0*rho;
    c   = cos(p);
    s   = sin(p);
    e   = exp((1.5+(double)m)*rho);
    tmp_in[i][0] = e*in[i]*c;
    tmp_in[i][1] = e*in[i]*s;
  }
  plan = fftw_plan_dft_1d(ngrid, tmp_in, tmp_out, 
                          FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
 
  /* the second transform */ 
  for (i=0; i<ngrid; i++) { 
    t = tmesh[i];
    p = rho0*(t-t0)+rho0*t; 
    c = cos(p);
    s = sin(p);
    im = (l*(l+1)+m)*ngrid+i;
    prod[0] = tmp_out[i][0]*Mmesh[im][0] - tmp_out[i][1]*Mmesh[im][1];
    prod[1] = tmp_out[i][0]*Mmesh[im][1] + tmp_out[i][1]*Mmesh[im][0];
    tmp_in[i][0] = drho*(prod[0]*c - prod[1]*s);
    tmp_in[i][1] = drho*(prod[0]*s + prod[1]*c);
  }
  fftw_execute(plan);
  fftw_destroy_plan(plan);
  
  for (i=0; i<ngrid; i++) { 
    kap = rhomesh[i];
    p   = t0*(kap-rho0); 
    c   = cos(p);
    s   = sin(p);
    e   = exp(((double)m-1.5)*kap)*dt;
    out[i] = e*(tmp_out[i][0]*c - tmp_out[i][1]*s);
    //out[2*i+1] = e*(tmp_out[i][0]*s + tmp_out[i][1]*c);
  }
}
#endif



int ERI_FSBT_ngrid(const ERI_FSBT_t *ptr) 
{
  return ptr->ngrid; 
} 


int ERI_FSBT_lmax(const ERI_FSBT_t *ptr) 
{ 
  return ptr->lmax; 
} 


double ERI_FSBT_Mesh_r(const ERI_FSBT_t *ptr, int i) 
{ 
  return ptr->r[i]; 
}


double ERI_FSBT_Mesh_k(const ERI_FSBT_t *ptr, int i)
{ 
  return ptr->r[i]; 
}


const double* ERI_FSBT_Mesh_Array_r(const ERI_FSBT_t *ptr) 
{ 
  return ptr->r; 
}


const double* ERI_FSBT_Mesh_Array_k(const ERI_FSBT_t *ptr)
{ 
  return ptr->r; 
}


double ERI_FSBT_Mesh_dr(const ERI_FSBT_t *ptr, int i)
{ 
  return ptr->r[i]*ptr->drho; 
}


double ERI_FSBT_Mesh_dk(const ERI_FSBT_t *ptr, int i)
{ 
  return ptr->r[i]*ptr->drho; 
}



/* EOF */
