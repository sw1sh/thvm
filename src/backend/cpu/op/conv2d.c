// backend/cpu/op/conv2d.c - 2-D convolution forward.
//
// Channels-first input {C_in, H, W}; weights {C_out, C_in, kh, kw};
// bias {C_out}.  Output {C_out, H - kh + 1, W - kw + 1}.  Stride 1,
// no padding, no dilation.
//
// p->arg packing (set in materialize_in_env.c):
//     bits 24..31 : kh
//     bits 16..23 : kw
//     bits  0..15 : W_out
// C_out = src_numels[2] (= bias numel);
// C_in  = src_numels[1] / (C_out * kh * kw);
// HW_out = out_numel / C_out;  H_out = HW_out / W_out.
//
// Layout indices (row-major):
//     input  [c_in, h, w]                 -> c_in*H*W + h*W + w
//     weight [c_out, c_in, dh, dw]        -> c_out*(C_in*kh*kw) + c_in*kh*kw + dh*kw + dw
//     output [c_out, h_out, w_out]        -> c_out*HW_out + h_out*W_out + w_out
// f32 only for v1 (DT_F32 is the standard tensor dtype).

fn void cpu_op_conv2d(void *out, void **srcs, u32 const *src_numels,
                      KProgOp const *p, u32 out_numel) {
  if (p->dtype != DT_F32) return;

  u32 kh    = (p->arg >> 24) & 0xFF;
  u32 kw    = (p->arg >> 16) & 0xFF;
  u32 w_out =  p->arg        & 0xFFFF;

  u32 c_out = src_numels[2];
  u32 hw_out = out_numel / c_out;
  u32 h_out = hw_out / w_out;
  u32 c_in  = src_numels[1] / (c_out * kh * kw);
  u32 h     = h_out + kh - 1;
  u32 w     = w_out + kw - 1;

  f32 const *input   = (f32 const *)srcs[0];
  f32 const *weights = (f32 const *)srcs[1];
  f32 const *bias    = (f32 const *)srcs[2];
  f32       *output  = (f32 *)out;

  for (u32 oc = 0; oc < c_out; oc++) {
    f32 b = bias[oc];
    for (u32 oh = 0; oh < h_out; oh++) {
      for (u32 ow = 0; ow < w_out; ow++) {
        f32 acc = b;
        for (u32 ic = 0; ic < c_in; ic++) {
          for (u32 dh = 0; dh < kh; dh++) {
            for (u32 dw = 0; dw < kw; dw++) {
              f32 x = input[ic * h * w + (oh + dh) * w + (ow + dw)];
              f32 wt = weights[oc * (c_in * kh * kw) + ic * (kh * kw) + dh * kw + dw];
              acc += x * wt;
            }
          }
        }
        output[oc * hw_out + oh * w_out + ow] = acc;
      }
    }
  }
}
