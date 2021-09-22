extern "C" void FUNC(UrstCubatureHex3D)(const dlong & Nelements,
  const dfloat * __restrict__ D,
  const dfloat * __restrict__ x,
  const dfloat * __restrict__ y,
  const dfloat * __restrict__ z,
  const dfloat * __restrict__ cubInterpT,
  const dfloat * __restrict__ cubW,
  const dlong & offset,
  const dlong & cubatureOffset,
  const dfloat * __restrict__ U,
  const dfloat * __restrict__ W,
  dfloat * __restrict__ result)
{
  dfloat r_cubxre[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubxse[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubxte[p_cubNq][p_cubNq][p_cubNq];

  dfloat r_cubyre[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubyse[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubyte[p_cubNq][p_cubNq][p_cubNq];

  dfloat r_cubzre[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubzse[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_cubzte[p_cubNq][p_cubNq][p_cubNq];

  dfloat s_cubInterpT[p_Nq][p_cubNq];
  dfloat s_D[p_Nq][p_Nq];

  dfloat s_U[p_cubNq][p_cubNq];
  dfloat s_V[p_cubNq][p_cubNq];
  dfloat s_W[p_cubNq][p_cubNq];

  dfloat s_U1[p_Nq][p_cubNq];
  dfloat s_V1[p_Nq][p_cubNq];
  dfloat s_W1[p_Nq][p_cubNq];

  dfloat r_U[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_V[p_cubNq][p_cubNq][p_cubNq];
  dfloat r_W[p_cubNq][p_cubNq][p_cubNq];

  dfloat s_x[p_Nq][p_Nq];
  dfloat s_y[p_Nq][p_Nq];
  dfloat s_z[p_Nq][p_Nq];

  dfloat r_x[p_Nq][p_Nq][p_Nq];
  dfloat r_y[p_Nq][p_Nq][p_Nq];
  dfloat r_z[p_Nq][p_Nq][p_Nq];

  dfloat s_xre[p_Nq][p_Nq];
  dfloat s_xse[p_Nq][p_Nq];
  dfloat s_xte[p_Nq][p_Nq];

  dfloat s_yre[p_Nq][p_Nq];
  dfloat s_yse[p_Nq][p_Nq];
  dfloat s_yte[p_Nq][p_Nq];
  
  dfloat s_zre[p_Nq][p_Nq];
  dfloat s_zse[p_Nq][p_Nq];
  dfloat s_zte[p_Nq][p_Nq];

  dfloat s_cubxre[p_Nq][p_cubNq];
  dfloat s_cubxse[p_Nq][p_cubNq];
  dfloat s_cubxte[p_Nq][p_cubNq];

  dfloat s_cubyre[p_Nq][p_cubNq];
  dfloat s_cubyse[p_Nq][p_cubNq];
  dfloat s_cubyte[p_Nq][p_cubNq];

  dfloat s_cubzre[p_Nq][p_cubNq];
  dfloat s_cubzse[p_Nq][p_cubNq];
  dfloat s_cubzte[p_Nq][p_cubNq];

  for(int j = 0; j < p_Nq; ++j) {
    for(int i = 0; i < p_cubNq; ++i) {
      const int id = i + j * p_cubNq;

      if(i < p_Nq){
          s_D[j][i]  = D[j*p_Nq+i];
      }

      s_cubInterpT[j][i] = cubInterpT[id];
    }
  }

  for(dlong element = 0; element < Nelements; ++element) {

    #pragma unroll
    for(int j = 0; j < p_cubNq; ++j) {
      #pragma unroll
      for(int i = 0; i < p_cubNq; ++i) {
        const int id = i + j * p_cubNq;
        #pragma unroll
        for(int k = 0; k < p_cubNq; ++k) {
          r_cubxre[j][i][k] = 0;
          r_cubxse[j][i][k] = 0;
          r_cubxte[j][i][k] = 0;

          r_cubyre[j][i][k] = 0;
          r_cubyse[j][i][k] = 0;
          r_cubyte[j][i][k] = 0;

          r_cubzre[j][i][k] = 0;
          r_cubzse[j][i][k] = 0;
          r_cubzte[j][i][k] = 0;

          r_U[j][i][k] = 0.0;
          r_V[j][i][k] = 0.0;
          r_W[j][i][k] = 0.0;
        }
      }
    }

    #pragma unroll
    for(int k = 0 ; k < p_Nq; ++k){
      #pragma unroll
      for(int j=0;j<p_Nq;++j){
        #pragma unroll
        for(int i=0;i<p_Nq;++i){
          const dlong id = element*p_Np + k*p_Nq*p_Nq + j*p_Nq + i;
          s_x[j][i] = x[id];
          s_y[j][i] = y[id];
          s_z[j][i] = z[id];
          if(k == 0){
            #pragma unroll
            for(int l = 0 ; l < p_Nq; ++l){
              const dlong other_id = element*p_Np + l*p_Nq*p_Nq + j*p_Nq + i;
              r_x[j][i][l] = x[other_id];
              r_y[j][i][l] = y[other_id];
              r_z[j][i][l] = z[other_id];
            }
          }
        }
      }

      #pragma unroll
      for(int j=0;j<p_Nq;++j){
        #pragma unroll
        for(int i=0;i<p_Nq;++i){
          dfloat xr = 0, yr = 0, zr = 0;
          dfloat xs = 0, ys = 0, zs = 0;
          dfloat xt = 0, yt = 0, zt = 0;
          #pragma unroll
          for(int m=0;m<p_Nq;++m){
            const dfloat Dim = s_D[i][m];
            const dfloat Djm = s_D[j][m];
            const dfloat Dkm = s_D[k][m];
            xr += Dim*s_x[j][m];
            xs += Djm*s_x[m][i];
            xt += Dkm*r_x[j][i][m];
            yr += Dim*s_y[j][m];
            ys += Djm*s_y[m][i];
            yt += Dkm*r_y[j][i][m];
            zr += Dim*s_z[j][m];
            zs += Djm*s_z[m][i];
            zt += Dkm*r_z[j][i][m];
          }
          // store results in shmem array
          s_xre[j][i] = xr;
          s_xse[j][i] = xs;
          s_xte[j][i] = xt;

          s_yre[j][i] = yr;
          s_yse[j][i] = ys;
          s_yte[j][i] = yt;

          s_zre[j][i] = zr;
          s_zse[j][i] = zs;
          s_zte[j][i] = zt;
        }
      }

      #pragma unroll
      for(int b = 0; b < p_Nq; ++b)
        #pragma unroll
        for(int i = 0; i < p_cubNq; ++i)
          {
            dfloat xr1  = 0, xs1 = 0,  xt1 = 0;
            dfloat yr1  = 0, ys1 = 0,  yt1 = 0;
            dfloat zr1  = 0, zs1 = 0,  zt1 = 0;
            #pragma unroll
            for(int a = 0; a < p_Nq; ++a) {
              dfloat Iia = s_cubInterpT[a][i];
              xr1  += Iia * s_xre[b][a];
              xs1  += Iia * s_xse[b][a];
              xt1  += Iia * s_xte[b][a];

              yr1  += Iia * s_yre[b][a];
              ys1  += Iia * s_yse[b][a];
              yt1  += Iia * s_yte[b][a];

              zr1  += Iia * s_zre[b][a];
              zs1  += Iia * s_zse[b][a];
              zt1  += Iia * s_zte[b][a];
            }

            s_cubxre[b][i] = xr1;
            s_cubxse[b][i] = xs1;
            s_cubxte[b][i] = xt1;

            s_cubyre[b][i] = yr1;
            s_cubyse[b][i] = ys1;
            s_cubyte[b][i] = yt1;

            s_cubzre[b][i] = zr1;
            s_cubzse[b][i] = zs1;
            s_cubzte[b][i] = zt1;

          }

      // interpolate in 's'
      #pragma unroll
      for(int j = 0; j < p_cubNq; ++j)
        #pragma unroll
        for(int i = 0; i < p_cubNq; ++i) {
          dfloat xr2  = 0, xs2 = 0,  xt2 = 0;
          dfloat yr2  = 0, ys2 = 0,  yt2 = 0;
          dfloat zr2  = 0, zs2 = 0,  zt2 = 0;
          // interpolate in b
          #pragma unroll
          for(int b = 0; b < p_Nq; ++b) {
            dfloat Ijb = s_cubInterpT[b][j];
            xr2 += Ijb * s_cubxre[b][i];
            xs2 += Ijb * s_cubxse[b][i];
            xt2 += Ijb * s_cubxte[b][i];

            yr2 += Ijb * s_cubyre[b][i];
            ys2 += Ijb * s_cubyse[b][i];
            yt2 += Ijb * s_cubyte[b][i];

            zr2 += Ijb * s_cubzre[b][i];
            zs2 += Ijb * s_cubzse[b][i];
            zt2 += Ijb * s_cubzte[b][i];
          }

          // interpolate in k progressively
          #pragma unroll
          for(int c = 0; c < p_cubNq; ++c) {
            dfloat Ick = s_cubInterpT[k][c];
            r_cubxre[j][i][c] += Ick * xr2;
            r_cubxse[j][i][c] += Ick * xs2;
            r_cubxte[j][i][c] += Ick * xt2;

            r_cubyre[j][i][c] += Ick * yr2;
            r_cubyse[j][i][c] += Ick * ys2;
            r_cubyte[j][i][c] += Ick * yt2;

            r_cubzre[j][i][c] += Ick * zr2;
            r_cubzse[j][i][c] += Ick * zs2;
            r_cubzte[j][i][c] += Ick * zt2;
          }
        }
    }

    #pragma unroll
    for(int c = 0; c < p_Nq; ++c) {

      #pragma unroll
      for(int b = 0; b < p_Nq; ++b)
        #pragma unroll
        for(int a = 0; a < p_Nq; ++a){
          const dlong id = element * p_Np + c * p_Nq * p_Nq + b * p_Nq + a;

          dfloat Ue = U[id + 0 * offset];
          dfloat Ve = U[id + 1 * offset];
          dfloat We = U[id + 2 * offset];
          if(p_relative){
            Ue -= W[id + 0 * offset];
            Ve -= W[id + 1 * offset];
            We -= W[id + 2 * offset];
          }

          s_U[b][a] = Ue;
          s_V[b][a] = Ve;
          s_W[b][a] = We;
        }

      // interpolate in 'r'
      #pragma unroll
      for(int b = 0; b < p_Nq; ++b)
        #pragma unroll
        for(int i = 0; i < p_cubNq; ++i)
          {
            dfloat U1  = 0, V1 = 0,  W1 = 0;

            #pragma unroll
            for(int a = 0; a < p_Nq; ++a) {
              dfloat Iia = s_cubInterpT[a][i];
              U1  += Iia * s_U[b][a];
              V1  += Iia * s_V[b][a];
              W1  += Iia * s_W[b][a];
            }

            s_U1[b][i] = U1;
            s_V1[b][i] = V1;
            s_W1[b][i] = W1;
          }

      // interpolate in 's'
      #pragma unroll
      for(int j = 0; j < p_cubNq; ++j) {
        #pragma unroll
        for(int i = 0; i < p_cubNq; ++i) {
          dfloat U2 = 0, V2 = 0,  W2 = 0;

          // interpolate in b
          #pragma unroll
          for(int b = 0; b < p_Nq; ++b) {
            dfloat Ijb = s_cubInterpT[b][j];
            U2 += Ijb * s_U1[b][i];
            V2 += Ijb * s_V1[b][i];
            W2 += Ijb * s_W1[b][i];
          }

          // interpolate in c progressively
          #pragma unroll
          for(int k = 0; k < p_cubNq; ++k) {
            dfloat Ikc = s_cubInterpT[c][k];

            r_U[j][i][k] += Ikc * U2;
            r_V[j][i][k] += Ikc * V2;
            r_W[j][i][k] += Ikc * W2;
          }
        }
      }
    }

    #pragma unroll
    for(int k = 0; k < p_cubNq; ++k) {
      for(int j = 0; j < p_cubNq; ++j)
        for(int i = 0; i < p_cubNq; ++i) {
          const dfloat xr = r_cubxre[j][i][k], xs = r_cubxse[j][i][k], xt = r_cubxte[j][i][k];
          const dfloat yr = r_cubyre[j][i][k], ys = r_cubyse[j][i][k], yt = r_cubyte[j][i][k];
          const dfloat zr = r_cubzre[j][i][k], zs = r_cubzse[j][i][k], zt = r_cubzte[j][i][k];

          const dfloat J = xr * (ys * zt - zs * yt) - yr * (xs * zt - zs * xt) + zr * (xs * yt - ys * xt);
          const dfloat invJ = 1.0 / J;

          const dfloat drdx =  (ys * zt - zs * yt) * invJ;
          const dfloat drdy = -(xs * zt - zs * xt) * invJ;
          const dfloat drdz =  (xs * yt - ys * xt) * invJ;

          const dfloat dsdx = -(yr * zt - zr * yt) * invJ;
          const dfloat dsdy =  (xr * zt - zr * xt) * invJ;
          const dfloat dsdz = -(xr * yt - yr * xt) * invJ;

          const dfloat dtdx =  (yr * zs - zr * ys) * invJ;
          const dfloat dtdy = -(xr * zs - zr * xs) * invJ;
          const dfloat dtdz =  (xr * ys - yr * xs) * invJ;

          const dfloat JW = J * cubW[i] * cubW[j] * cubW[k];

          const dfloat Un = r_U[j][i][k];
          const dfloat Vn = r_V[j][i][k];
          const dfloat Wn = r_W[j][i][k];

          const dfloat Uhat = JW * (Un * drdx + Vn * drdy + Wn * drdz);
          const dfloat Vhat = JW * (Un * dsdx + Vn * dsdy + Wn * dsdz);
          const dfloat What = JW * (Un * dtdx + Vn * dtdy + Wn * dtdz);

          const dlong id = element * p_cubNp + k * p_cubNq * p_cubNq + j * p_cubNq + i;
          result[id + 0 * cubatureOffset] = Uhat;
          result[id + 1 * cubatureOffset] = Vhat;
          result[id + 2 * cubatureOffset] = What;
        }
    }
  }
}