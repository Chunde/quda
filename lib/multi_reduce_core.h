/**
   Driver for multi-blas with up to five vectors
*/
template<int NXZ, typename doubleN, typename ReduceType,
  template <int MXZ, typename ReducerType, typename Float, typename FloatN> class Reducer,
  int writeX, int writeY, int writeZ, int writeW, bool siteUnroll, typename T>
  void multiReduceCuda(doubleN result[], const reduce::coeff_array<T> &a, const reduce::coeff_array<T> &b, const reduce::coeff_array<T> &c,
		       CompositeColorSpinorField& x, CompositeColorSpinorField& y,
		       CompositeColorSpinorField& z, CompositeColorSpinorField& w){
  const int NYW = y.size();

  if (NYW != 1 && NYW != NXZ)
  {
    errorQuda("multi-reduce requires NYW == 1 or NXZ");
  }

  int reduce_length = siteUnroll ? x[0]->RealLength() : x[0]->Length();

  if (x[0]->Precision() == QUDA_DOUBLE_PRECISION) {
    if (x[0]->Nspin() == 4 || x[0]->Nspin() == 2) { // wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC) || defined(GPU_MULTIGRID)
      const int M = siteUnroll ? 12 : 1; // determines how much work per thread to do
      if (x[0]->Nspin() == 2 && siteUnroll) errorQuda("siteUnroll not supported for nSpin==2");
      multiReduceCuda<doubleN,ReduceType,double2,double2,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, reduce_length/(2*M));
#else
      errorQuda("blas has not been built for Nspin=%d fields", x[0]->Nspin());
#endif
    } else if (x[0]->Nspin() == 1) {
#ifdef GPU_STAGGERED_DIRAC
      const int M = siteUnroll ? 3 : 1; // determines how much work per thread to do
      multiReduceCuda<doubleN,ReduceType,double2,double2,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, reduce_length/(2*M));
#else
      errorQuda("blas has not been built for Nspin=%d field", x[0]->Nspin());
#endif
    } else { errorQuda("nSpin=%d is not supported\n", x[0]->Nspin()); }
  } else if (x[0]->Precision() == QUDA_SINGLE_PRECISION) {
    if (x[0]->Nspin() == 4) { // wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC)
      const int M = siteUnroll ? 6 : 1; // determines how much work per thread to do
      multiReduceCuda<doubleN,ReduceType,float4,float4,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, reduce_length/(4*M));
#else
      errorQuda("blas has not been built for Nspin=%d fields", x[0]->Nspin());
#endif
    } else if(x[0]->Nspin() == 1 || x[0]->Nspin() == 2) { // staggered
#if defined(GPU_STAGGERED_DIRAC) || defined(GPU_MULTIGRID)
      const int M = siteUnroll ? 3 : 1;
      if (x[0]->Nspin() == 2 && siteUnroll) errorQuda("siteUnroll not supported for nSpin==2");
      multiReduceCuda<doubleN,ReduceType,float2,float2,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, reduce_length/(2*M));
#else
      errorQuda("blas has not been built for Nspin=%d fields", x[0]->Nspin());
#endif
    } else { errorQuda("nSpin=%d is not supported\n", x[0]->Nspin()); }
  } else { // half precision
    if (x[0]->Nspin() == 4) { // wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC)
      const int M = 6;
      multiReduceCuda<doubleN,ReduceType,float4,short4,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, x[0]->Volume());
#else
      errorQuda("blas has not been built for Nspin=%d fields", x[0]->Nspin());
#endif
    } else if(x[0]->Nspin() == 1) { // staggered
#ifdef GPU_STAGGERED_DIRAC
      const int M = 3;
      multiReduceCuda<doubleN,ReduceType,float2,short2,M,NXZ,Reducer,writeX,writeY,writeZ,writeW>
	(result, a, b, c, x, y, z, w, x[0]->Volume());
#else
      errorQuda("blas has not been built for Nspin=%d fields", x[0]->Nspin());
#endif
    } else { errorQuda("nSpin=%d is not supported\n", x[0]->Nspin()); }
  }

  // now do multi-node reduction
  const int Nreduce = NXZ*NYW*(sizeof(doubleN)/sizeof(double));
  reduceDoubleArray((double*)result, Nreduce);

  return;
}
