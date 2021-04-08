#map = affine_map<(d0) -> (d0 + 1)>
func @syrk_128(%alpha: f32, %beta: f32, %C: memref<128x128xf32>, %A: memref<128x128xf32>) {
  affine.for %i = 0 to 128 {
    affine.for %j = 0 to #map(%i) {
      %0 = affine.load %C[%i, %j] : memref<128x128xf32>
      %1 = mulf %beta, %0 : f32
      affine.store %1, %C[%i, %j] : memref<128x128xf32>
      affine.for %k = 0 to 128 {
        %2 = affine.load %A[%i, %k] : memref<128x128xf32>
        %3 = affine.load %A[%j, %k] : memref<128x128xf32>
        %4 = affine.load %C[%i, %j] : memref<128x128xf32>
        %5 = mulf %alpha, %2 : f32
        %6 = mulf %5, %3 : f32
        %7 = addf %4, %6 : f32
        affine.store %7, %C[%i, %j] : memref<128x128xf32>
      }
    }
  }
  return
}
