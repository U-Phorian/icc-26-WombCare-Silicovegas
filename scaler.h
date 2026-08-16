#ifndef WOMBCARE_SCALER_H
#define WOMBCARE_SCALER_H
#define WOMBCARE_N_FEATURES 8
extern const float g_feat_mean[WOMBCARE_N_FEATURES];
extern const float g_feat_std[WOMBCARE_N_FEATURES];
extern const float g_in_scale; extern const int g_in_zp;
extern const float g_out_scale; extern const int g_out_zp;
#endif
