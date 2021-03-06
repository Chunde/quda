#include <typeinfo>
#include <quda_internal.h>
#include <lattice_field.h>
#include <color_spinor_field.h>
#include <gauge_field.h>
#include <clover_field.h>
#include <face_quda.h>

namespace quda {

  void* LatticeField::bufferPinned[] = {NULL};
  bool LatticeField::bufferPinnedInit[] = {false};
  size_t LatticeField::bufferPinnedBytes[] = {0};
  size_t LatticeField::bufferPinnedResizeCount = 0;

  void* LatticeField::bufferDevice = NULL;
  bool LatticeField::bufferDeviceInit = false;
  size_t LatticeField::bufferDeviceBytes = 0;

  LatticeFieldParam::LatticeFieldParam(const LatticeField &field)
    : nDim(field.Ndim()), pad(field.Pad()), precision(field.Precision()),
      siteSubset(field.SiteSubset()), ghostExchange(field.GhostExchange())
  {
    for(int dir=0; dir<nDim; ++dir) {
      x[dir] = field.X()[dir];
      r[dir] = field.R()[dir];
    }
  }

  LatticeField::LatticeField(const LatticeFieldParam &param)
    : volume(1), pad(param.pad), total_bytes(0), nDim(param.nDim), precision(param.precision),
      siteSubset(param.siteSubset), ghostExchange(param.ghostExchange)
  {
    for (int i=0; i<nDim; i++) {
      x[i] = param.x[i];
      r[i] = ghostExchange == QUDA_GHOST_EXCHANGE_EXTENDED ? param.r[i] : 0;
      volume *= param.x[i];
      surface[i] = 1;
      for (int j=0; j<nDim; j++) {
	if (i==j) continue;
	surface[i] *= param.x[j];
      }
    }

    if (siteSubset == QUDA_INVALID_SITE_SUBSET) errorQuda("siteSubset is not set");
    volumeCB = (siteSubset == QUDA_FULL_SITE_SUBSET) ? volume / 2 : volume;
    stride = volumeCB + pad;
  
    // for parity fields the factor of half is present for all surfaces dimensions except x, so add it manually
    for (int i=0; i<nDim; i++) 
      surfaceCB[i] = (siteSubset == QUDA_FULL_SITE_SUBSET || i==0) ? surface[i] / 2 : surface[i];

    // for 5-dimensional fields, we only communicate in the space-time dimensions
    nDimComms = nDim == 5 ? 4 : nDim;

    setTuningString();
  }

  LatticeField::~LatticeField() { }

  void LatticeField::setTuningString() {
    char vol_tmp[TuneKey::volume_n];
    int check;
    check = snprintf(vol_string, TuneKey::volume_n, "%d", x[0]);
    if (check < 0 || check >= TuneKey::volume_n) errorQuda("Error writing volume string");
    for (int d=1; d<nDim; d++) {
      strcpy(vol_tmp, vol_string);
      check = snprintf(vol_string, TuneKey::volume_n, "%sx%d", vol_tmp, x[d]);
      if (check < 0 || check >= TuneKey::volume_n) errorQuda("Error writing volume string");
    }
  }

  void LatticeField::checkField(const LatticeField &a) const {
    if (a.nDim != nDim) errorQuda("nDim does not match %d %d", nDim, a.nDim);
    if (ghostExchange != QUDA_GHOST_EXCHANGE_EXTENDED && a.ghostExchange == QUDA_GHOST_EXCHANGE_EXTENDED) {
      // if source is extended by I am not then we need to compare their interior volume to my volume
      int a_volume_interior = 1;
      for (int i=0; i<nDim; i++) {
	if (a.x[i]-2*a.r[i] != x[i]) errorQuda("x[%d] does not match %d %d", i, x[i], a.x[i]-2*a.r[i]);
	a_volume_interior *= a.x[i] - 2*a.r[i];
      }
      if (a_volume_interior != volume) errorQuda("Interior volume does not match %d %d", volume, a_volume_interior);
    } else if (a.ghostExchange != QUDA_GHOST_EXCHANGE_EXTENDED && ghostExchange == QUDA_GHOST_EXCHANGE_EXTENDED) {
      // if source is extended by I am not then we need to compare their interior volume to my volume
      int this_volume_interior = 1;
      for (int i=0; i<nDim; i++) {
	if (x[i]-2*r[i] != a.x[i]) errorQuda("x[%d] does not match %d %d", i, x[i]-2*r[i], a.x[i]);
	this_volume_interior *= x[i] - 2*r[i];
      }
      if (this_volume_interior != a.volume) errorQuda("Interior volume does not match %d %d", this_volume_interior, a.volume);
    } else {
      if (a.volume != volume) errorQuda("Volume does not match %d %d", volume, a.volume);
      if (a.volumeCB != volumeCB) errorQuda("VolumeCB does not match %d %d", volumeCB, a.volumeCB);
      for (int i=0; i<nDim; i++) {
	if (a.x[i] != x[i]) errorQuda("x[%d] does not match %d %d", i, x[i], a.x[i]);
	if (a.surface[i] != surface[i]) errorQuda("surface[%d] does not match %d %d", i, surface[i], a.surface[i]);
	if (a.surfaceCB[i] != surfaceCB[i]) errorQuda("surfaceCB[%d] does not match %d %d", i, surfaceCB[i], a.surfaceCB[i]);
      }
    }
  }

  QudaFieldLocation LatticeField::Location() const { 
    QudaFieldLocation location = QUDA_INVALID_FIELD_LOCATION;
    if (typeid(*this)==typeid(cudaCloverField) || 
	typeid(*this)==typeid(cudaColorSpinorField) ||
	typeid(*this)==typeid(cudaGaugeField)) {
      location = QUDA_CUDA_FIELD_LOCATION; 
    } else if (typeid(*this)==typeid(cpuCloverField) || 
	       typeid(*this)==typeid(cpuColorSpinorField) ||
	       typeid(*this)==typeid(cpuGaugeField)) {
      location = QUDA_CPU_FIELD_LOCATION;
      location = QUDA_CPU_FIELD_LOCATION;
    } else {
      errorQuda("Unknown field %s, so cannot determine location", typeid(*this).name());
    }
    return location;
}

  void LatticeField::read(char *filename) {
    errorQuda("Not implemented");
  }
  
  void LatticeField::write(char *filename) {
    errorQuda("Not implemented");
  }

  int LatticeField::Nvec() const {
    if (typeid(*this) == typeid(const cudaColorSpinorField)) {
      const ColorSpinorField &csField = static_cast<const ColorSpinorField&>(*this);
      if (csField.FieldOrder() == 2 || csField.FieldOrder() == 4)
	return static_cast<int>(csField.FieldOrder());
    } else if (typeid(*this) == typeid(const cudaGaugeField)) {
      const GaugeField &gField = static_cast<const GaugeField&>(*this);
      if (gField.Order() == 2 || gField.Order() == 4)
	return static_cast<int>(gField.Order());
    } else if (typeid(*this) == typeid(const cudaCloverField)) { 
      const CloverField &cField = static_cast<const CloverField&>(*this);
      if (cField.Order() == 2 || cField.Order() == 4)
	return static_cast<int>(cField.Order());
    }

    errorQuda("Unsupported field type");
    return -1;
  }

  void LatticeField::resizeBufferPinned(size_t bytes, const int idx) const {
    if ((bytes > bufferPinnedBytes[idx] || bufferPinnedInit[idx] == 0) && bytes > 0) {
      if (bufferPinnedInit[idx]) host_free(bufferPinned[idx]);
      bufferPinned[idx] = pinned_malloc(bytes);
      bufferPinnedBytes[idx] = bytes;
      bufferPinnedInit[idx] = true;
      bufferPinnedResizeCount++;
      if (bufferPinnedResizeCount == 0) bufferPinnedResizeCount = 1; // keep 0 as initialization state
    }
  }

  void LatticeField::resizeBufferDevice(size_t bytes) const {
    if ((bytes > bufferDeviceBytes || bufferDeviceInit == 0) && bytes > 0) {
      if (bufferDeviceInit) device_free(bufferDevice);
      bufferDevice = device_malloc(bytes);
      bufferDeviceBytes = bytes;
      bufferDeviceInit = true;
    }
  }

  void LatticeField::freeBuffer(int index) {
    if (bufferPinnedInit[index]) {
      host_free(bufferPinned[index]);
      bufferPinned[index] = NULL;
      bufferPinnedBytes[index] = 0;
      bufferPinnedInit[index] = false;
    }
    if (bufferDeviceInit) {
      device_free(bufferDevice);
      bufferDevice = NULL;
      bufferDeviceBytes = 0;
      bufferDeviceInit = false;
    }
  }

  // This doesn't really live here, but is fine for the moment
  std::ostream& operator<<(std::ostream& output, const LatticeFieldParam& param)
  {
    output << "nDim = " << param.nDim << std::endl;
    for (int i=0; i<param.nDim; i++) {
      output << "x[" << i << "] = " << param.x[i] << std::endl;    
    }
    output << "pad = " << param.pad << std::endl;
    output << "precision = " << param.precision << std::endl;

    output << "ghostExchange = " << param.ghostExchange << std::endl;
    for (int i=0; i<param.nDim; i++) {
      output << "r[" << i << "] = " << param.r[i] << std::endl;
    }

    return output;  // for multiple << operators.
  }

  static QudaFieldLocation reorder_location_ = QUDA_CUDA_FIELD_LOCATION;

  QudaFieldLocation reorder_location() { return reorder_location_; }
  void reorder_location_set(QudaFieldLocation _reorder_location) { reorder_location_ = _reorder_location; }

} // namespace quda
