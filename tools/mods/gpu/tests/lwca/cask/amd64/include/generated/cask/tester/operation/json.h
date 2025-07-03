// AUTOMATICALLY GENERATED, DO NOT EDIT MANUALLY!!!
// clang-format off
//   DATE: 01/26/2022 21:54:18
//   PYTHONPATH: /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/cask_core:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/bladeworks:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/decorator-src/src:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/six-src:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/pytools-src:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/pymbolic-src:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/jinja-src/src:/home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/markupsafe-src/src
//   CMD: /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/cask_core/cask_plugin/scripts/codegen.py --spec /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/cask_core/cask_plugin/specs/graph.py --template /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/cask_core/cask_plugin/templates/tester/json.h.j2 --template-dir /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/cask_core/cask_plugin/templates --output /home/scratch.bmiranda_sw_1/p4-2001/dev/compute_arch/src/fast_kernels/git/cask_sdk/_deps/cask_core-build/cask_plugin/include/generated/cask/tester/operation/json.h --namespace cask_core --only .*
// clang-format on

#pragma once

#include <cask/tester/json.h>
#include <cask/tester/exception.h>
#include <cask/tester/debug.h>

#include <generated/cask_plugin/operation.h>

namespace cask_plugin {
////////////////////////////////////////////////////////////
template <typename JSON>
inline void to_json(JSON &j, const Matrix& op) {

  {
    JSON j_struct;
    j_struct["batches"] = op.stride.batches;
    j_struct["rows"] = op.stride.rows;
    j_struct["cols"] = op.stride.cols;
    j["stride"] = j_struct;
  }
  j["batches"] = op.batches;
  j["rows"] = op.rows;
  j["cols"] = op.cols;
  j["size_in_bytes"] = op.size_in_bytes;
  j["size_in_elements"] = op.size_in_elements;
  j["dimensions"] = op.dimensions;
}

template<typename JSON>
inline void from_json(const JSON& j, Matrix& p) {


  static std::set<std::string> allowed_keys = {
    "name", "type", "fill",
    "batches",
    "rows",
    "cols",
    "size_in_bytes",
    "size_in_elements",
    "dimensions",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("batches")) { j.at("batches").get_to(p.batches); }
  if (j.contains("rows")) { j.at("rows").get_to(p.rows); }
  if (j.contains("cols")) { j.at("cols").get_to(p.cols); }
  if (j.contains("size_in_bytes")) { j.at("size_in_bytes").get_to(p.size_in_bytes); }
  if (j.contains("size_in_elements")) { j.at("size_in_elements").get_to(p.size_in_elements); }
  if (j.contains("dimensions")) { j.at("dimensions").get_to(p.dimensions); }


}
template <typename JSON>
inline void to_json(JSON &j, const Activation& op) {

  {
    JSON j_struct;
    j_struct["depth"] = op.stride.depth;
    j_struct["height"] = op.stride.height;
    j_struct["width"] = op.stride.width;
    j_struct["channels"] = op.stride.channels;
    j_struct["batches"] = op.stride.batches;
    j["stride"] = j_struct;
  }
  j["depth"] = op.depth;
  j["height"] = op.height;
  j["width"] = op.width;
  j["channels"] = op.channels;
  j["batches"] = op.batches;
  j["size_in_bytes"] = op.size_in_bytes;
  j["size_in_elements"] = op.size_in_elements;
  j["dimensions"] = op.dimensions;
}

template<typename JSON>
inline void from_json(const JSON& j, Activation& p) {


  static std::set<std::string> allowed_keys = {
    "name", "type", "fill",
    "depth",
    "height",
    "width",
    "channels",
    "batches",
    "size_in_bytes",
    "size_in_elements",
    "dimensions",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("depth")) { j.at("depth").get_to(p.depth); }
  if (j.contains("height")) { j.at("height").get_to(p.height); }
  if (j.contains("width")) { j.at("width").get_to(p.width); }
  if (j.contains("channels")) { j.at("channels").get_to(p.channels); }
  if (j.contains("batches")) { j.at("batches").get_to(p.batches); }
  if (j.contains("size_in_bytes")) { j.at("size_in_bytes").get_to(p.size_in_bytes); }
  if (j.contains("size_in_elements")) { j.at("size_in_elements").get_to(p.size_in_elements); }
  if (j.contains("dimensions")) { j.at("dimensions").get_to(p.dimensions); }


}
template <typename JSON>
inline void to_json(JSON &j, const Filter& op) {

  {
    JSON j_struct;
    j_struct["depth"] = op.stride.depth;
    j_struct["height"] = op.stride.height;
    j_struct["width"] = op.stride.width;
    j_struct["channels"] = op.stride.channels;
    j_struct["filters"] = op.stride.filters;
    j["stride"] = j_struct;
  }
  j["depth"] = op.depth;
  j["height"] = op.height;
  j["width"] = op.width;
  j["channels"] = op.channels;
  j["filters"] = op.filters;
  j["size_in_bytes"] = op.size_in_bytes;
  j["size_in_elements"] = op.size_in_elements;
  j["dimensions"] = op.dimensions;
}

template<typename JSON>
inline void from_json(const JSON& j, Filter& p) {


  static std::set<std::string> allowed_keys = {
    "name", "type", "fill",
    "depth",
    "height",
    "width",
    "channels",
    "filters",
    "size_in_bytes",
    "size_in_elements",
    "dimensions",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("depth")) { j.at("depth").get_to(p.depth); }
  if (j.contains("height")) { j.at("height").get_to(p.height); }
  if (j.contains("width")) { j.at("width").get_to(p.width); }
  if (j.contains("channels")) { j.at("channels").get_to(p.channels); }
  if (j.contains("filters")) { j.at("filters").get_to(p.filters); }
  if (j.contains("size_in_bytes")) { j.at("size_in_bytes").get_to(p.size_in_bytes); }
  if (j.contains("size_in_elements")) { j.at("size_in_elements").get_to(p.size_in_elements); }
  if (j.contains("dimensions")) { j.at("dimensions").get_to(p.dimensions); }


}
template <typename JSON>
inline void to_json(JSON &j, const Vector& op) {

  {
    JSON j_struct;
    j_struct["elems"] = op.stride.elems;
    j["stride"] = j_struct;
  }
  j["elems"] = op.elems;
  j["size_in_bytes"] = op.size_in_bytes;
  j["size_in_elements"] = op.size_in_elements;
  j["dimensions"] = op.dimensions;
}

template<typename JSON>
inline void from_json(const JSON& j, Vector& p) {


  static std::set<std::string> allowed_keys = {
    "name", "type", "fill",
    "elems",
    "size_in_bytes",
    "size_in_elements",
    "dimensions",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("elems")) { j.at("elems").get_to(p.elems); }
  if (j.contains("size_in_bytes")) { j.at("size_in_bytes").get_to(p.size_in_bytes); }
  if (j.contains("size_in_elements")) { j.at("size_in_elements").get_to(p.size_in_elements); }
  if (j.contains("dimensions")) { j.at("dimensions").get_to(p.dimensions); }


}
template <typename JSON>
inline void to_json(JSON &j, const Convolution& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["bias"], *cask_plugin::toInternalShape(&op.bias));

  to_json(j["bias_data"], op.bias_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  {
    JSON j_struct;
    j_struct["slices"] = op.split_k.slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["m"] = op.cga_tile.m;
    j_struct["n"] = op.cga_tile.n;
    j_struct["k"] = op.cga_tile.k;
    j["cga_tile"] = j_struct;
  }
  j["groups"] = op.groups;
  j["padding_front"] = op.padding_front;
  j["padding_back"] = op.padding_back;
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["padding_value"] = op.padding_value;
  j["stride_depth"] = op.stride_depth;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["dilation_depth"] = op.dilation_depth;
  j["dilation_height"] = op.dilation_height;
  j["dilation_width"] = op.dilation_width;
  j["is_xcorrelation"] = op.is_xcorrelation;
  j["with_bias"] = op.with_bias;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["apply_relu"] = op.apply_relu;
  j["relu_upper_bound"] = op.relu_upper_bound;
  j["relu_lower_bound"] = op.relu_lower_bound;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["split_k_t"] = op.split_k_t;
  j["split_k_r"] = op.split_k_r;
  j["ctas_per_wave"] = op.ctas_per_wave;
  j["cta_swizzle"] = op.cta_swizzle;
  j["runtime_para0"] = op.runtime_para0;
}

template<typename JSON>
inline void from_json(const JSON& j, Convolution& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "y",
    "x",
    "w",
    "c",
    "bias",
    "valpha",
    "vbeta",
    "groups",
    "padding_front",
    "padding_back",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "padding_value",
    "stride_depth",
    "stride_height",
    "stride_width",
    "dilation_depth",
    "dilation_height",
    "dilation_width",
    "is_xcorrelation",
    "with_bias",
    "alpha",
    "beta",
    "apply_relu",
    "relu_upper_bound",
    "relu_lower_bound",
    "per_channel_scaling",
    "split_k_t",
    "split_k_r",
    "ctas_per_wave",
    "cta_swizzle",
    "runtime_para0",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("bias")) {
    j.at("bias").get_to(*cask_plugin::toInternalShape(&p.bias));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("groups")) { j.at("groups").get_to(p.groups); }
  if (j.contains("padding_front")) { j.at("padding_front").get_to(p.padding_front); }
  if (j.contains("padding_back")) { j.at("padding_back").get_to(p.padding_back); }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("padding_value")) { j.at("padding_value").get_to(p.padding_value); }
  if (j.contains("stride_depth")) { j.at("stride_depth").get_to(p.stride_depth); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("dilation_depth")) { j.at("dilation_depth").get_to(p.dilation_depth); }
  if (j.contains("dilation_height")) { j.at("dilation_height").get_to(p.dilation_height); }
  if (j.contains("dilation_width")) { j.at("dilation_width").get_to(p.dilation_width); }
  if (j.contains("is_xcorrelation")) { j.at("is_xcorrelation").get_to(p.is_xcorrelation); }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("relu_upper_bound")) { j.at("relu_upper_bound").get_to(p.relu_upper_bound); }
  if (j.contains("relu_lower_bound")) { j.at("relu_lower_bound").get_to(p.relu_lower_bound); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("split_k_t")) { j.at("split_k_t").get_to(p.split_k_t); }
  if (j.contains("split_k_r")) { j.at("split_k_r").get_to(p.split_k_r); }
  if (j.contains("ctas_per_wave")) { j.at("ctas_per_wave").get_to(p.ctas_per_wave); }
  if (j.contains("cta_swizzle")) { j.at("cta_swizzle").get_to(p.cta_swizzle); }
  if (j.contains("runtime_para0")) { j.at("runtime_para0").get_to(p.runtime_para0); }


}
template <typename JSON>
inline void to_json(JSON &j, const ConvolutionDgrad& op) {

  to_json(j["dy"], *cask_plugin::toInternalShape(&op.dy));

  to_json(j["dy_data"], op.dy_data);
  to_json(j["dx"], *cask_plugin::toInternalShape(&op.dx));

  to_json(j["dx_data"], op.dx_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  {
    JSON j_struct;
    j_struct["slices"] = op.split_k.slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["m"] = op.cga_tile.m;
    j_struct["n"] = op.cga_tile.n;
    j_struct["k"] = op.cga_tile.k;
    j["cga_tile"] = j_struct;
  }
  j["groups"] = op.groups;
  j["padding_front"] = op.padding_front;
  j["padding_back"] = op.padding_back;
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["stride_depth"] = op.stride_depth;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["dilation_depth"] = op.dilation_depth;
  j["dilation_height"] = op.dilation_height;
  j["dilation_width"] = op.dilation_width;
  j["is_xcorrelation"] = op.is_xcorrelation;
  j["with_bias"] = op.with_bias;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["apply_relu"] = op.apply_relu;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["split_k_t"] = op.split_k_t;
  j["split_k_r"] = op.split_k_r;
  j["ctas_per_wave"] = op.ctas_per_wave;
  j["cta_swizzle"] = op.cta_swizzle;
}

template<typename JSON>
inline void from_json(const JSON& j, ConvolutionDgrad& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "dy",
    "dx",
    "w",
    "c",
    "valpha",
    "vbeta",
    "groups",
    "padding_front",
    "padding_back",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "stride_depth",
    "stride_height",
    "stride_width",
    "dilation_depth",
    "dilation_height",
    "dilation_width",
    "is_xcorrelation",
    "with_bias",
    "alpha",
    "beta",
    "apply_relu",
    "per_channel_scaling",
    "split_k_t",
    "split_k_r",
    "ctas_per_wave",
    "cta_swizzle",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("dy")) {
    j.at("dy").get_to(*cask_plugin::toInternalShape(&p.dy));
  }
  if (j.contains("dx")) {
    j.at("dx").get_to(*cask_plugin::toInternalShape(&p.dx));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("groups")) { j.at("groups").get_to(p.groups); }
  if (j.contains("padding_front")) { j.at("padding_front").get_to(p.padding_front); }
  if (j.contains("padding_back")) { j.at("padding_back").get_to(p.padding_back); }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("stride_depth")) { j.at("stride_depth").get_to(p.stride_depth); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("dilation_depth")) { j.at("dilation_depth").get_to(p.dilation_depth); }
  if (j.contains("dilation_height")) { j.at("dilation_height").get_to(p.dilation_height); }
  if (j.contains("dilation_width")) { j.at("dilation_width").get_to(p.dilation_width); }
  if (j.contains("is_xcorrelation")) { j.at("is_xcorrelation").get_to(p.is_xcorrelation); }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("split_k_t")) { j.at("split_k_t").get_to(p.split_k_t); }
  if (j.contains("split_k_r")) { j.at("split_k_r").get_to(p.split_k_r); }
  if (j.contains("ctas_per_wave")) { j.at("ctas_per_wave").get_to(p.ctas_per_wave); }
  if (j.contains("cta_swizzle")) { j.at("cta_swizzle").get_to(p.cta_swizzle); }


}
template <typename JSON>
inline void to_json(JSON &j, const ConvolutionWgrad& op) {

  to_json(j["dy"], *cask_plugin::toInternalShape(&op.dy));

  to_json(j["dy_data"], op.dy_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["dw"], *cask_plugin::toInternalShape(&op.dw));

  to_json(j["dw_data"], op.dw_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  {
    JSON j_struct;
    j_struct["split_k_slices"] = op.split_k.split_k_slices;
    j_struct["split_h_slices"] = op.split_k.split_h_slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["m"] = op.cga_tile.m;
    j_struct["n"] = op.cga_tile.n;
    j_struct["k"] = op.cga_tile.k;
    j["cga_tile"] = j_struct;
  }
  j["groups"] = op.groups;
  j["padding_front"] = op.padding_front;
  j["padding_back"] = op.padding_back;
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["stride_depth"] = op.stride_depth;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["dilation_depth"] = op.dilation_depth;
  j["dilation_height"] = op.dilation_height;
  j["dilation_width"] = op.dilation_width;
  j["is_xcorrelation"] = op.is_xcorrelation;
  j["with_bias"] = op.with_bias;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["apply_relu"] = op.apply_relu;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["split_k_t"] = op.split_k_t;
  j["split_k_r"] = op.split_k_r;
}

template<typename JSON>
inline void from_json(const JSON& j, ConvolutionWgrad& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "dy",
    "x",
    "dw",
    "c",
    "groups",
    "padding_front",
    "padding_back",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "stride_depth",
    "stride_height",
    "stride_width",
    "dilation_depth",
    "dilation_height",
    "dilation_width",
    "is_xcorrelation",
    "with_bias",
    "alpha",
    "beta",
    "apply_relu",
    "per_channel_scaling",
    "split_k_t",
    "split_k_r",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("dy")) {
    j.at("dy").get_to(*cask_plugin::toInternalShape(&p.dy));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("dw")) {
    j.at("dw").get_to(*cask_plugin::toInternalShape(&p.dw));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("groups")) { j.at("groups").get_to(p.groups); }
  if (j.contains("padding_front")) { j.at("padding_front").get_to(p.padding_front); }
  if (j.contains("padding_back")) { j.at("padding_back").get_to(p.padding_back); }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("stride_depth")) { j.at("stride_depth").get_to(p.stride_depth); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("dilation_depth")) { j.at("dilation_depth").get_to(p.dilation_depth); }
  if (j.contains("dilation_height")) { j.at("dilation_height").get_to(p.dilation_height); }
  if (j.contains("dilation_width")) { j.at("dilation_width").get_to(p.dilation_width); }
  if (j.contains("is_xcorrelation")) { j.at("is_xcorrelation").get_to(p.is_xcorrelation); }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("split_k_t")) { j.at("split_k_t").get_to(p.split_k_t); }
  if (j.contains("split_k_r")) { j.at("split_k_r").get_to(p.split_k_r); }


}
template <typename JSON>
inline void to_json(JSON &j, const Deconvolution& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["bias"], *cask_plugin::toInternalShape(&op.bias));

  to_json(j["bias_data"], op.bias_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  {
    JSON j_struct;
    j_struct["slices"] = op.split_k.slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["m"] = op.cga_tile.m;
    j_struct["n"] = op.cga_tile.n;
    j_struct["k"] = op.cga_tile.k;
    j["cga_tile"] = j_struct;
  }
  j["groups"] = op.groups;
  j["padding_front"] = op.padding_front;
  j["padding_back"] = op.padding_back;
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["stride_depth"] = op.stride_depth;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["dilation_depth"] = op.dilation_depth;
  j["dilation_height"] = op.dilation_height;
  j["dilation_width"] = op.dilation_width;
  j["is_xcorrelation"] = op.is_xcorrelation;
  j["with_bias"] = op.with_bias;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["apply_relu"] = op.apply_relu;
  j["relu_upper_bound"] = op.relu_upper_bound;
  j["relu_lower_bound"] = op.relu_lower_bound;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["split_k_t"] = op.split_k_t;
  j["split_k_r"] = op.split_k_r;
  j["ctas_per_wave"] = op.ctas_per_wave;
  j["cta_swizzle"] = op.cta_swizzle;
  j["runtime_para0"] = op.runtime_para0;
}

template<typename JSON>
inline void from_json(const JSON& j, Deconvolution& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "y",
    "x",
    "w",
    "c",
    "bias",
    "valpha",
    "vbeta",
    "groups",
    "padding_front",
    "padding_back",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "stride_depth",
    "stride_height",
    "stride_width",
    "dilation_depth",
    "dilation_height",
    "dilation_width",
    "is_xcorrelation",
    "with_bias",
    "alpha",
    "beta",
    "apply_relu",
    "relu_upper_bound",
    "relu_lower_bound",
    "per_channel_scaling",
    "split_k_t",
    "split_k_r",
    "ctas_per_wave",
    "cta_swizzle",
    "runtime_para0",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("bias")) {
    j.at("bias").get_to(*cask_plugin::toInternalShape(&p.bias));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("groups")) { j.at("groups").get_to(p.groups); }
  if (j.contains("padding_front")) { j.at("padding_front").get_to(p.padding_front); }
  if (j.contains("padding_back")) { j.at("padding_back").get_to(p.padding_back); }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("stride_depth")) { j.at("stride_depth").get_to(p.stride_depth); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("dilation_depth")) { j.at("dilation_depth").get_to(p.dilation_depth); }
  if (j.contains("dilation_height")) { j.at("dilation_height").get_to(p.dilation_height); }
  if (j.contains("dilation_width")) { j.at("dilation_width").get_to(p.dilation_width); }
  if (j.contains("is_xcorrelation")) { j.at("is_xcorrelation").get_to(p.is_xcorrelation); }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("relu_upper_bound")) { j.at("relu_upper_bound").get_to(p.relu_upper_bound); }
  if (j.contains("relu_lower_bound")) { j.at("relu_lower_bound").get_to(p.relu_lower_bound); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("split_k_t")) { j.at("split_k_t").get_to(p.split_k_t); }
  if (j.contains("split_k_r")) { j.at("split_k_r").get_to(p.split_k_r); }
  if (j.contains("ctas_per_wave")) { j.at("ctas_per_wave").get_to(p.ctas_per_wave); }
  if (j.contains("cta_swizzle")) { j.at("cta_swizzle").get_to(p.cta_swizzle); }
  if (j.contains("runtime_para0")) { j.at("runtime_para0").get_to(p.runtime_para0); }


}
template <typename JSON>
inline void to_json(JSON &j, const Pooling& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["win_height"] = op.win_height;
  j["win_width"] = op.win_width;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["blend_factor"] = op.blend_factor;
  j["pooling_mode"] = op.pooling_mode;
  j["per_channel_scaling"] = op.per_channel_scaling;
}

template<typename JSON>
inline void from_json(const JSON& j, Pooling& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "y",
    "x",
    "valpha",
    "vbeta",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "stride_height",
    "stride_width",
    "win_height",
    "win_width",
    "alpha",
    "beta",
    "blend_factor",
    "pooling_mode",
    "per_channel_scaling",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("win_height")) { j.at("win_height").get_to(p.win_height); }
  if (j.contains("win_width")) { j.at("win_width").get_to(p.win_width); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("blend_factor")) { j.at("blend_factor").get_to(p.blend_factor); }
  if (j.contains("pooling_mode")) { j.at("pooling_mode").get_to(p.pooling_mode); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }


}
template <typename JSON>
inline void to_json(JSON &j, const Softmax& op) {

  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  j["dim"] = op.dim;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["per_channel_scaling"] = op.per_channel_scaling;
}

template<typename JSON>
inline void from_json(const JSON& j, Softmax& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "x",
    "y",
    "dim",
    "alpha",
    "beta",
    "per_channel_scaling",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("dim")) { j.at("dim").get_to(p.dim); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }


}
template <typename JSON>
inline void to_json(JSON &j, const Reduce& op) {

  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["partial_sum"], *cask_plugin::toInternalShape(&op.partial_sum));

  to_json(j["partial_sum_data"], op.partial_sum_data);
  to_json(j["partial_sum_square"], *cask_plugin::toInternalShape(&op.partial_sum_square));

  to_json(j["partial_sum_square_data"], op.partial_sum_square_data);
  to_json(j["gamma"], *cask_plugin::toInternalShape(&op.gamma));

  to_json(j["gamma_data"], op.gamma_data);
  to_json(j["beta"], *cask_plugin::toInternalShape(&op.beta));

  to_json(j["beta_data"], op.beta_data);
  j["inv_count"] = op.inv_count;
  j["epsilon"] = op.epsilon;
  j["dqScaleIn"] = op.dqScaleIn;
  j["qScale"] = op.qScale;
  j["num_elements_per_partial_sum"] = op.num_elements_per_partial_sum;
  j["num_partial_sums"] = op.num_partial_sums;
  j["M"] = op.M;
  j["N"] = op.N;
}

template<typename JSON>
inline void from_json(const JSON& j, Reduce& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "x",
    "y",
    "partial_sum",
    "partial_sum_square",
    "gamma",
    "beta",
    "inv_count",
    "epsilon",
    "dqScaleIn",
    "qScale",
    "num_elements_per_partial_sum",
    "num_partial_sums",
    "M",
    "N",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("partial_sum")) {
    j.at("partial_sum").get_to(*cask_plugin::toInternalShape(&p.partial_sum));
  }
  if (j.contains("partial_sum_square")) {
    j.at("partial_sum_square").get_to(*cask_plugin::toInternalShape(&p.partial_sum_square));
  }
  if (j.contains("gamma")) {
    j.at("gamma").get_to(*cask_plugin::toInternalShape(&p.gamma));
  }
  if (j.contains("beta")) {
    j.at("beta").get_to(*cask_plugin::toInternalShape(&p.beta));
  }
  if (j.contains("inv_count")) { j.at("inv_count").get_to(p.inv_count); }
  if (j.contains("epsilon")) { j.at("epsilon").get_to(p.epsilon); }
  if (j.contains("dqScaleIn")) { j.at("dqScaleIn").get_to(p.dqScaleIn); }
  if (j.contains("qScale")) { j.at("qScale").get_to(p.qScale); }
  if (j.contains("num_elements_per_partial_sum")) { j.at("num_elements_per_partial_sum").get_to(p.num_elements_per_partial_sum); }
  if (j.contains("num_partial_sums")) { j.at("num_partial_sums").get_to(p.num_partial_sums); }
  if (j.contains("M")) { j.at("M").get_to(p.M); }
  if (j.contains("N")) { j.at("N").get_to(p.N); }


}
template <typename JSON>
inline void to_json(JSON &j, const ReLU& op) {

  to_json(j["x"], op.x);

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], op.y);

  to_json(j["y_data"], op.y_data);
  j["relu_lower_bound"] = op.relu_lower_bound;
  j["relu_upper_bound"] = op.relu_upper_bound;
}

template<typename JSON>
inline void from_json(const JSON& j, ReLU& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "x",
    "y",
    "relu_lower_bound",
    "relu_upper_bound",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("x")) {
    j.at("x").get_to(p.x);
  }
  if (j.contains("y")) {
    j.at("y").get_to(p.y);
  }
  if (j.contains("relu_lower_bound")) { j.at("relu_lower_bound").get_to(p.relu_lower_bound); }
  if (j.contains("relu_upper_bound")) { j.at("relu_upper_bound").get_to(p.relu_upper_bound); }


}
template <typename JSON>
inline void to_json(JSON &j, const MatMul& op) {

  to_json(j["a"], *cask_plugin::toInternalShape(&op.a));

  to_json(j["a_data"], op.a_data);
  to_json(j["b"], *cask_plugin::toInternalShape(&op.b));

  to_json(j["b_data"], op.b_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["f"], op.f);

  to_json(j["f_data"], op.f_data);
}

template<typename JSON>
inline void from_json(const JSON& j, MatMul& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "a",
    "b",
    "c",
    "f",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("a")) {
    j.at("a").get_to(*cask_plugin::toInternalShape(&p.a));
  }
  if (j.contains("b")) {
    j.at("b").get_to(*cask_plugin::toInternalShape(&p.b));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("f")) {
    j.at("f").get_to(p.f);
  }


}
template <typename JSON>
inline void to_json(JSON &j, const Add& op) {

  to_json(j["c"], op.c);

  to_json(j["c_data"], op.c_data);
  to_json(j["x"], op.x);

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], op.y);

  to_json(j["y_data"], op.y_data);
}

template<typename JSON>
inline void from_json(const JSON& j, Add& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "c",
    "x",
    "y",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("c")) {
    j.at("c").get_to(p.c);
  }
  if (j.contains("x")) {
    j.at("x").get_to(p.x);
  }
  if (j.contains("y")) {
    j.at("y").get_to(p.y);
  }


}
template <typename JSON>
inline void to_json(JSON &j, const Mul& op) {

  to_json(j["x"], op.x);

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], op.y);

  to_json(j["y_data"], op.y_data);
  to_json(j["c"], op.c);

  to_json(j["c_data"], op.c_data);
  j["scale"] = op.scale;
  j["scale_imag"] = op.scale_imag;
}

template<typename JSON>
inline void from_json(const JSON& j, Mul& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "x",
    "y",
    "c",
    "scale",
    "scale_imag",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("x")) {
    j.at("x").get_to(p.x);
  }
  if (j.contains("y")) {
    j.at("y").get_to(p.y);
  }
  if (j.contains("c")) {
    j.at("c").get_to(p.c);
  }
  if (j.contains("scale")) { j.at("scale").get_to(p.scale); }
  if (j.contains("scale_imag")) { j.at("scale_imag").get_to(p.scale_imag); }


}
template <typename JSON>
inline void to_json(JSON &j, const Conv& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  j["groups"] = op.groups;
  j["padding_front"] = op.padding_front;
  j["padding_back"] = op.padding_back;
  j["padding_top"] = op.padding_top;
  j["padding_bottom"] = op.padding_bottom;
  j["padding_left"] = op.padding_left;
  j["padding_right"] = op.padding_right;
  j["stride_depth"] = op.stride_depth;
  j["stride_height"] = op.stride_height;
  j["stride_width"] = op.stride_width;
  j["dilation_depth"] = op.dilation_depth;
  j["dilation_height"] = op.dilation_height;
  j["dilation_width"] = op.dilation_width;
  j["is_xcorrelation"] = op.is_xcorrelation;
}

template<typename JSON>
inline void from_json(const JSON& j, Conv& p) {


  static std::set<std::string> allowed_keys = {
    "name", "tensor_bindings", "shader", "stream", "type",
    "y",
    "x",
    "w",
    "groups",
    "padding_front",
    "padding_back",
    "padding_top",
    "padding_bottom",
    "padding_left",
    "padding_right",
    "stride_depth",
    "stride_height",
    "stride_width",
    "dilation_depth",
    "dilation_height",
    "dilation_width",
    "is_xcorrelation",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("groups")) { j.at("groups").get_to(p.groups); }
  if (j.contains("padding_front")) { j.at("padding_front").get_to(p.padding_front); }
  if (j.contains("padding_back")) { j.at("padding_back").get_to(p.padding_back); }
  if (j.contains("padding_top")) { j.at("padding_top").get_to(p.padding_top); }
  if (j.contains("padding_bottom")) { j.at("padding_bottom").get_to(p.padding_bottom); }
  if (j.contains("padding_left")) { j.at("padding_left").get_to(p.padding_left); }
  if (j.contains("padding_right")) { j.at("padding_right").get_to(p.padding_right); }
  if (j.contains("stride_depth")) { j.at("stride_depth").get_to(p.stride_depth); }
  if (j.contains("stride_height")) { j.at("stride_height").get_to(p.stride_height); }
  if (j.contains("stride_width")) { j.at("stride_width").get_to(p.stride_width); }
  if (j.contains("dilation_depth")) { j.at("dilation_depth").get_to(p.dilation_depth); }
  if (j.contains("dilation_height")) { j.at("dilation_height").get_to(p.dilation_height); }
  if (j.contains("dilation_width")) { j.at("dilation_width").get_to(p.dilation_width); }
  if (j.contains("is_xcorrelation")) { j.at("is_xcorrelation").get_to(p.is_xcorrelation); }


}
template <typename JSON>
inline void to_json(JSON &j, const Elementwise& op) {

  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
}

template<typename JSON>
inline void from_json(const JSON& j, Elementwise& p) {


  static std::set<std::string> allowed_keys = {
    "x",
    "y",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }


}
template <typename JSON>
inline void to_json(JSON &j, const LayoutTransformation& op) {

  to_json(j["A"], op.A);

  to_json(j["A_data"], op.A_data);
  to_json(j["B"], op.B);

  to_json(j["B_data"], op.B_data);
  to_json(j["C"], op.C);

  to_json(j["C_data"], op.C_data);
  to_json(j["D"], op.D);

  to_json(j["D_data"], op.D_data);
  j["Alpha"] = op.Alpha;
  j["Beta"] = op.Beta;
  j["Gamma"] = op.Gamma;
  j["OpA"] = op.OpA;
  j["OpB"] = op.OpB;
  j["OpC"] = op.OpC;
  j["OpAB"] = op.OpAB;
  j["OpABC"] = op.OpABC;
}

template<typename JSON>
inline void from_json(const JSON& j, LayoutTransformation& p) {


  static std::set<std::string> allowed_keys = {
    "A",
    "B",
    "C",
    "D",
    "Alpha",
    "Beta",
    "Gamma",
    "OpA",
    "OpB",
    "OpC",
    "OpAB",
    "OpABC",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("A")) {
    j.at("A").get_to(p.A);
  }
  if (j.contains("B")) {
    j.at("B").get_to(p.B);
  }
  if (j.contains("C")) {
    j.at("C").get_to(p.C);
  }
  if (j.contains("D")) {
    j.at("D").get_to(p.D);
  }
  if (j.contains("Alpha")) { j.at("Alpha").get_to(p.Alpha); }
  if (j.contains("Beta")) { j.at("Beta").get_to(p.Beta); }
  if (j.contains("Gamma")) { j.at("Gamma").get_to(p.Gamma); }
  if (j.contains("OpA")) { j.at("OpA").get_to(p.OpA); }
  if (j.contains("OpB")) { j.at("OpB").get_to(p.OpB); }
  if (j.contains("OpC")) { j.at("OpC").get_to(p.OpC); }
  if (j.contains("OpAB")) { j.at("OpAB").get_to(p.OpAB); }
  if (j.contains("OpABC")) { j.at("OpABC").get_to(p.OpABC); }


}
template <typename JSON>
inline void to_json(JSON &j, const Gett& op) {

  to_json(j["a"], op.a);

  to_json(j["a_data"], op.a_data);
  to_json(j["b"], op.b);

  to_json(j["b_data"], op.b_data);
  to_json(j["c"], op.c);

  to_json(j["c_data"], op.c_data);
  to_json(j["d"], op.d);

  to_json(j["d_data"], op.d_data);
  {
    JSON j_struct;
    j_struct["slices"] = op.split_k.slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  j["num_modes_l"] = op.num_modes_l;
  j["num_modes_m"] = op.num_modes_m;
  j["num_modes_n"] = op.num_modes_n;
  j["num_modes_k"] = op.num_modes_k;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
}

template<typename JSON>
inline void from_json(const JSON& j, Gett& p) {


  static std::set<std::string> allowed_keys = {
    "a",
    "b",
    "c",
    "d",
    "num_modes_l",
    "num_modes_m",
    "num_modes_n",
    "num_modes_k",
    "alpha",
    "beta",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("a")) {
    j.at("a").get_to(p.a);
  }
  if (j.contains("b")) {
    j.at("b").get_to(p.b);
  }
  if (j.contains("c")) {
    j.at("c").get_to(p.c);
  }
  if (j.contains("d")) {
    j.at("d").get_to(p.d);
  }
  if (j.contains("num_modes_l")) { j.at("num_modes_l").get_to(p.num_modes_l); }
  if (j.contains("num_modes_m")) { j.at("num_modes_m").get_to(p.num_modes_m); }
  if (j.contains("num_modes_n")) { j.at("num_modes_n").get_to(p.num_modes_n); }
  if (j.contains("num_modes_k")) { j.at("num_modes_k").get_to(p.num_modes_k); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }


}
template <typename JSON>
inline void to_json(JSON &j, const AddMul4& op) {

  to_json(j["a"], op.a);

  to_json(j["a_data"], op.a_data);
  to_json(j["b"], op.b);

  to_json(j["b_data"], op.b_data);
  to_json(j["c"], op.c);

  to_json(j["c_data"], op.c_data);
  to_json(j["d"], op.d);

  to_json(j["d_data"], op.d_data);
  to_json(j["y"], op.y);

  to_json(j["y_data"], op.y_data);
  to_json(j["valpha"], op.valpha);

  to_json(j["valpha_data"], op.valpha_data);
  j["scaling.scale"] = op.scaling.scale;
  j["scaling.scale_imag"] = op.scaling.scale_imag;
}

template<typename JSON>
inline void from_json(const JSON& j, AddMul4& p) {


  static std::set<std::string> allowed_keys = {
    "a",
    "b",
    "c",
    "d",
    "y",
    "valpha",
    "scaling.scale",
    "scaling.scale_imag",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("a")) {
    j.at("a").get_to(p.a);
  }
  if (j.contains("b")) {
    j.at("b").get_to(p.b);
  }
  if (j.contains("c")) {
    j.at("c").get_to(p.c);
  }
  if (j.contains("d")) {
    j.at("d").get_to(p.d);
  }
  if (j.contains("y")) {
    j.at("y").get_to(p.y);
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(p.valpha);
  }
  if (j.contains("scaling.scale")) { j.at("scaling.scale").get_to(p.scaling.scale); }
  if (j.contains("scaling.scale_imag")) { j.at("scaling.scale_imag").get_to(p.scaling.scale_imag); }


}
template <typename JSON>
inline void to_json(JSON &j, const ConvBiasReLU& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["bias"], *cask_plugin::toInternalShape(&op.bias));

  to_json(j["bias_data"], op.bias_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  j["with_bias"] = op.with_bias;
  j["apply_relu"] = op.apply_relu;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["conv.groups"] = op.conv.groups;
  j["conv.padding_front"] = op.conv.padding_front;
  j["conv.padding_back"] = op.conv.padding_back;
  j["conv.padding_top"] = op.conv.padding_top;
  j["conv.padding_bottom"] = op.conv.padding_bottom;
  j["conv.padding_left"] = op.conv.padding_left;
  j["conv.padding_right"] = op.conv.padding_right;
  j["conv.stride_depth"] = op.conv.stride_depth;
  j["conv.stride_height"] = op.conv.stride_height;
  j["conv.stride_width"] = op.conv.stride_width;
  j["conv.dilation_depth"] = op.conv.dilation_depth;
  j["conv.dilation_height"] = op.conv.dilation_height;
  j["conv.dilation_width"] = op.conv.dilation_width;
  j["conv.is_xcorrelation"] = op.conv.is_xcorrelation;
  j["relu.relu_lower_bound"] = op.relu.relu_lower_bound;
  j["relu.relu_upper_bound"] = op.relu.relu_upper_bound;
  j["alpha_scaling.scale"] = op.alpha_scaling.scale;
  j["alpha_scaling.scale_imag"] = op.alpha_scaling.scale_imag;
  j["beta_scaling.scale"] = op.beta_scaling.scale;
  j["beta_scaling.scale_imag"] = op.beta_scaling.scale_imag;
}

template<typename JSON>
inline void from_json(const JSON& j, ConvBiasReLU& p) {


  static std::set<std::string> allowed_keys = {
    "y",
    "x",
    "w",
    "c",
    "bias",
    "valpha",
    "vbeta",
    "with_bias",
    "apply_relu",
    "per_channel_scaling",
    "conv.groups",
    "conv.padding_front",
    "conv.padding_back",
    "conv.padding_top",
    "conv.padding_bottom",
    "conv.padding_left",
    "conv.padding_right",
    "conv.stride_depth",
    "conv.stride_height",
    "conv.stride_width",
    "conv.dilation_depth",
    "conv.dilation_height",
    "conv.dilation_width",
    "conv.is_xcorrelation",
    "relu.relu_lower_bound",
    "relu.relu_upper_bound",
    "alpha_scaling.scale",
    "alpha_scaling.scale_imag",
    "beta_scaling.scale",
    "beta_scaling.scale_imag",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("bias")) {
    j.at("bias").get_to(*cask_plugin::toInternalShape(&p.bias));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("conv.groups")) { j.at("conv.groups").get_to(p.conv.groups); }
  if (j.contains("conv.padding_front")) { j.at("conv.padding_front").get_to(p.conv.padding_front); }
  if (j.contains("conv.padding_back")) { j.at("conv.padding_back").get_to(p.conv.padding_back); }
  if (j.contains("conv.padding_top")) { j.at("conv.padding_top").get_to(p.conv.padding_top); }
  if (j.contains("conv.padding_bottom")) { j.at("conv.padding_bottom").get_to(p.conv.padding_bottom); }
  if (j.contains("conv.padding_left")) { j.at("conv.padding_left").get_to(p.conv.padding_left); }
  if (j.contains("conv.padding_right")) { j.at("conv.padding_right").get_to(p.conv.padding_right); }
  if (j.contains("conv.stride_depth")) { j.at("conv.stride_depth").get_to(p.conv.stride_depth); }
  if (j.contains("conv.stride_height")) { j.at("conv.stride_height").get_to(p.conv.stride_height); }
  if (j.contains("conv.stride_width")) { j.at("conv.stride_width").get_to(p.conv.stride_width); }
  if (j.contains("conv.dilation_depth")) { j.at("conv.dilation_depth").get_to(p.conv.dilation_depth); }
  if (j.contains("conv.dilation_height")) { j.at("conv.dilation_height").get_to(p.conv.dilation_height); }
  if (j.contains("conv.dilation_width")) { j.at("conv.dilation_width").get_to(p.conv.dilation_width); }
  if (j.contains("conv.is_xcorrelation")) { j.at("conv.is_xcorrelation").get_to(p.conv.is_xcorrelation); }
  if (j.contains("relu.relu_lower_bound")) { j.at("relu.relu_lower_bound").get_to(p.relu.relu_lower_bound); }
  if (j.contains("relu.relu_upper_bound")) { j.at("relu.relu_upper_bound").get_to(p.relu.relu_upper_bound); }
  if (j.contains("alpha_scaling.scale")) { j.at("alpha_scaling.scale").get_to(p.alpha_scaling.scale); }
  if (j.contains("alpha_scaling.scale_imag")) { j.at("alpha_scaling.scale_imag").get_to(p.alpha_scaling.scale_imag); }
  if (j.contains("beta_scaling.scale")) { j.at("beta_scaling.scale").get_to(p.beta_scaling.scale); }
  if (j.contains("beta_scaling.scale_imag")) { j.at("beta_scaling.scale_imag").get_to(p.beta_scaling.scale_imag); }


}
template <typename JSON>
inline void to_json(JSON &j, const GemmBiasReLU& op) {

  to_json(j["d"], *cask_plugin::toInternalShape(&op.d));

  to_json(j["d_data"], op.d_data);
  to_json(j["a"], *cask_plugin::toInternalShape(&op.a));

  to_json(j["a_data"], op.a_data);
  to_json(j["b"], *cask_plugin::toInternalShape(&op.b));

  to_json(j["b_data"], op.b_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["bias"], *cask_plugin::toInternalShape(&op.bias));

  to_json(j["bias_data"], op.bias_data);
  to_json(j["valpha"], *cask_plugin::toInternalShape(&op.valpha));

  to_json(j["valpha_data"], op.valpha_data);
  to_json(j["vbeta"], *cask_plugin::toInternalShape(&op.vbeta));

  to_json(j["vbeta_data"], op.vbeta_data);
  to_json(j["scale_a"], *cask_plugin::toInternalShape(&op.scale_a));

  to_json(j["scale_a_data"], op.scale_a_data);
  to_json(j["scale_b"], *cask_plugin::toInternalShape(&op.scale_b));

  to_json(j["scale_b_data"], op.scale_b_data);
  to_json(j["scale_c"], *cask_plugin::toInternalShape(&op.scale_c));

  to_json(j["scale_c_data"], op.scale_c_data);
  to_json(j["scale_d"], *cask_plugin::toInternalShape(&op.scale_d));

  to_json(j["scale_d_data"], op.scale_d_data);
  to_json(j["amax_d"], *cask_plugin::toInternalShape(&op.amax_d));

  to_json(j["amax_d_data"], op.amax_d_data);
  {
    JSON j_struct;
    j_struct["slices"] = op.split_k.slices;
    j_struct["kernels"] = op.split_k.kernels;
    j_struct["buffers"] = op.split_k.buffers;
    j["split_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["split1"] = op.segment_k.split1;
    j_struct["split2"] = op.segment_k.split2;
    j_struct["factor1"] = op.segment_k.factor1;
    j_struct["factor2"] = op.segment_k.factor2;
    j_struct["gmem_limit"] = op.segment_k.gmem_limit;
    j_struct["k_min"] = op.segment_k.k_min;
    j_struct["factor_min"] = op.segment_k.factor_min;
    j_struct["factor_max"] = op.segment_k.factor_max;
    j["segment_k"] = j_struct;
  }
  {
    JSON j_struct;
    j_struct["m"] = op.cga_tile.m;
    j_struct["n"] = op.cga_tile.n;
    j_struct["k"] = op.cga_tile.k;
    j["cga_tile"] = j_struct;
  }
  j["apply_relu"] = op.apply_relu;
  j["with_bias"] = op.with_bias;
  j["pass_device_pointer"] = op.pass_device_pointer;
  j["array_count"] = op.array_count;
  j["per_channel_scaling"] = op.per_channel_scaling;
  j["ctas_per_wave"] = op.ctas_per_wave;
  j["cta_swizzle"] = op.cta_swizzle;
  j["runtime_para0"] = op.runtime_para0;
  j["period_mask_ns"] = op.period_mask_ns;
  j["pulse_ns"] = op.pulse_ns;
  j["min_sleep"] = op.min_sleep;
  j["mainloop_count"] = op.mainloop_count;
  j["relu.relu_lower_bound"] = op.relu.relu_lower_bound;
  j["relu.relu_upper_bound"] = op.relu.relu_upper_bound;
  j["alpha_scaling.scale"] = op.alpha_scaling.scale;
  j["alpha_scaling.scale_imag"] = op.alpha_scaling.scale_imag;
  j["beta_scaling.scale"] = op.beta_scaling.scale;
  j["beta_scaling.scale_imag"] = op.beta_scaling.scale_imag;
}

template<typename JSON>
inline void from_json(const JSON& j, GemmBiasReLU& p) {


  static std::set<std::string> allowed_keys = {
    "d",
    "a",
    "b",
    "c",
    "bias",
    "valpha",
    "vbeta",
    "scale_a",
    "scale_b",
    "scale_c",
    "scale_d",
    "amax_d",
    "apply_relu",
    "with_bias",
    "pass_device_pointer",
    "array_count",
    "per_channel_scaling",
    "ctas_per_wave",
    "cta_swizzle",
    "runtime_para0",
    "period_mask_ns",
    "pulse_ns",
    "min_sleep",
    "mainloop_count",
    "relu.relu_lower_bound",
    "relu.relu_upper_bound",
    "alpha_scaling.scale",
    "alpha_scaling.scale_imag",
    "beta_scaling.scale",
    "beta_scaling.scale_imag",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("d")) {
    j.at("d").get_to(*cask_plugin::toInternalShape(&p.d));
  }
  if (j.contains("a")) {
    j.at("a").get_to(*cask_plugin::toInternalShape(&p.a));
  }
  if (j.contains("b")) {
    j.at("b").get_to(*cask_plugin::toInternalShape(&p.b));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("bias")) {
    j.at("bias").get_to(*cask_plugin::toInternalShape(&p.bias));
  }
  if (j.contains("valpha")) {
    j.at("valpha").get_to(*cask_plugin::toInternalShape(&p.valpha));
  }
  if (j.contains("vbeta")) {
    j.at("vbeta").get_to(*cask_plugin::toInternalShape(&p.vbeta));
  }
  if (j.contains("scale_a")) {
    j.at("scale_a").get_to(*cask_plugin::toInternalShape(&p.scale_a));
  }
  if (j.contains("scale_b")) {
    j.at("scale_b").get_to(*cask_plugin::toInternalShape(&p.scale_b));
  }
  if (j.contains("scale_c")) {
    j.at("scale_c").get_to(*cask_plugin::toInternalShape(&p.scale_c));
  }
  if (j.contains("scale_d")) {
    j.at("scale_d").get_to(*cask_plugin::toInternalShape(&p.scale_d));
  }
  if (j.contains("amax_d")) {
    j.at("amax_d").get_to(*cask_plugin::toInternalShape(&p.amax_d));
  }
  if (j.contains("apply_relu")) { j.at("apply_relu").get_to(p.apply_relu); }
  if (j.contains("with_bias")) { j.at("with_bias").get_to(p.with_bias); }
  if (j.contains("pass_device_pointer")) { j.at("pass_device_pointer").get_to(p.pass_device_pointer); }
  if (j.contains("array_count")) { j.at("array_count").get_to(p.array_count); }
  if (j.contains("per_channel_scaling")) { j.at("per_channel_scaling").get_to(p.per_channel_scaling); }
  if (j.contains("ctas_per_wave")) { j.at("ctas_per_wave").get_to(p.ctas_per_wave); }
  if (j.contains("cta_swizzle")) { j.at("cta_swizzle").get_to(p.cta_swizzle); }
  if (j.contains("runtime_para0")) { j.at("runtime_para0").get_to(p.runtime_para0); }
  if (j.contains("period_mask_ns")) { j.at("period_mask_ns").get_to(p.period_mask_ns); }
  if (j.contains("pulse_ns")) { j.at("pulse_ns").get_to(p.pulse_ns); }
  if (j.contains("min_sleep")) { j.at("min_sleep").get_to(p.min_sleep); }
  if (j.contains("mainloop_count")) { j.at("mainloop_count").get_to(p.mainloop_count); }
  if (j.contains("relu.relu_lower_bound")) { j.at("relu.relu_lower_bound").get_to(p.relu.relu_lower_bound); }
  if (j.contains("relu.relu_upper_bound")) { j.at("relu.relu_upper_bound").get_to(p.relu.relu_upper_bound); }
  if (j.contains("alpha_scaling.scale")) { j.at("alpha_scaling.scale").get_to(p.alpha_scaling.scale); }
  if (j.contains("alpha_scaling.scale_imag")) { j.at("alpha_scaling.scale_imag").get_to(p.alpha_scaling.scale_imag); }
  if (j.contains("beta_scaling.scale")) { j.at("beta_scaling.scale").get_to(p.beta_scaling.scale); }
  if (j.contains("beta_scaling.scale_imag")) { j.at("beta_scaling.scale_imag").get_to(p.beta_scaling.scale_imag); }


}
template <typename JSON>
inline void to_json(JSON &j, const DepSepConv& op) {

  to_json(j["dep_a"], *cask_plugin::toInternalShape(&op.dep_a));

  to_json(j["dep_a_data"], op.dep_a_data);
  to_json(j["dep_b"], *cask_plugin::toInternalShape(&op.dep_b));

  to_json(j["dep_b_data"], op.dep_b_data);
  to_json(j["sep_b"], *cask_plugin::toInternalShape(&op.sep_b));

  to_json(j["sep_b_data"], op.sep_b_data);
  to_json(j["dep_bias"], *cask_plugin::toInternalShape(&op.dep_bias));

  to_json(j["dep_bias_data"], op.dep_bias_data);
  to_json(j["sep_bias"], *cask_plugin::toInternalShape(&op.sep_bias));

  to_json(j["sep_bias_data"], op.sep_bias_data);
  to_json(j["dep_alpha"], *cask_plugin::toInternalShape(&op.dep_alpha));

  to_json(j["dep_alpha_data"], op.dep_alpha_data);
  to_json(j["sep_alpha"], *cask_plugin::toInternalShape(&op.sep_alpha));

  to_json(j["sep_alpha_data"], op.sep_alpha_data);
  to_json(j["sep_c"], *cask_plugin::toInternalShape(&op.sep_c));

  to_json(j["sep_c_data"], op.sep_c_data);
  j["dep_conv.groups"] = op.dep_conv.groups;
  j["dep_conv.padding_front"] = op.dep_conv.padding_front;
  j["dep_conv.padding_back"] = op.dep_conv.padding_back;
  j["dep_conv.padding_top"] = op.dep_conv.padding_top;
  j["dep_conv.padding_bottom"] = op.dep_conv.padding_bottom;
  j["dep_conv.padding_left"] = op.dep_conv.padding_left;
  j["dep_conv.padding_right"] = op.dep_conv.padding_right;
  j["dep_conv.stride_depth"] = op.dep_conv.stride_depth;
  j["dep_conv.stride_height"] = op.dep_conv.stride_height;
  j["dep_conv.stride_width"] = op.dep_conv.stride_width;
  j["dep_conv.dilation_depth"] = op.dep_conv.dilation_depth;
  j["dep_conv.dilation_height"] = op.dep_conv.dilation_height;
  j["dep_conv.dilation_width"] = op.dep_conv.dilation_width;
  j["dep_conv.is_xcorrelation"] = op.dep_conv.is_xcorrelation;
  j["dep_with_relu"] = op.dep_with_relu;
  j["dep_relu.relu_lower_bound"] = op.dep_relu.relu_lower_bound;
  j["dep_relu.relu_upper_bound"] = op.dep_relu.relu_upper_bound;
  j["dep_scale.scale"] = op.dep_scale.scale;
  j["dep_scale.scale_imag"] = op.dep_scale.scale_imag;
  j["sep_conv.groups"] = op.sep_conv.groups;
  j["sep_conv.padding_front"] = op.sep_conv.padding_front;
  j["sep_conv.padding_back"] = op.sep_conv.padding_back;
  j["sep_conv.padding_top"] = op.sep_conv.padding_top;
  j["sep_conv.padding_bottom"] = op.sep_conv.padding_bottom;
  j["sep_conv.padding_left"] = op.sep_conv.padding_left;
  j["sep_conv.padding_right"] = op.sep_conv.padding_right;
  j["sep_conv.stride_depth"] = op.sep_conv.stride_depth;
  j["sep_conv.stride_height"] = op.sep_conv.stride_height;
  j["sep_conv.stride_width"] = op.sep_conv.stride_width;
  j["sep_conv.dilation_depth"] = op.sep_conv.dilation_depth;
  j["sep_conv.dilation_height"] = op.sep_conv.dilation_height;
  j["sep_conv.dilation_width"] = op.sep_conv.dilation_width;
  j["sep_conv.is_xcorrelation"] = op.sep_conv.is_xcorrelation;
  j["sep_with_relu"] = op.sep_with_relu;
  j["sep_relu.relu_lower_bound"] = op.sep_relu.relu_lower_bound;
  j["sep_relu.relu_upper_bound"] = op.sep_relu.relu_upper_bound;
  j["sep_scale.scale"] = op.sep_scale.scale;
  j["sep_scale.scale_imag"] = op.sep_scale.scale_imag;
}

template<typename JSON>
inline void from_json(const JSON& j, DepSepConv& p) {


  static std::set<std::string> allowed_keys = {
    "dep_a",
    "dep_b",
    "sep_b",
    "dep_bias",
    "sep_bias",
    "dep_alpha",
    "sep_alpha",
    "sep_c",
    "dep_conv.groups",
    "dep_conv.padding_front",
    "dep_conv.padding_back",
    "dep_conv.padding_top",
    "dep_conv.padding_bottom",
    "dep_conv.padding_left",
    "dep_conv.padding_right",
    "dep_conv.stride_depth",
    "dep_conv.stride_height",
    "dep_conv.stride_width",
    "dep_conv.dilation_depth",
    "dep_conv.dilation_height",
    "dep_conv.dilation_width",
    "dep_conv.is_xcorrelation",
    "dep_with_relu",
    "dep_relu.relu_lower_bound",
    "dep_relu.relu_upper_bound",
    "dep_scale.scale",
    "dep_scale.scale_imag",
    "sep_conv.groups",
    "sep_conv.padding_front",
    "sep_conv.padding_back",
    "sep_conv.padding_top",
    "sep_conv.padding_bottom",
    "sep_conv.padding_left",
    "sep_conv.padding_right",
    "sep_conv.stride_depth",
    "sep_conv.stride_height",
    "sep_conv.stride_width",
    "sep_conv.dilation_depth",
    "sep_conv.dilation_height",
    "sep_conv.dilation_width",
    "sep_conv.is_xcorrelation",
    "sep_with_relu",
    "sep_relu.relu_lower_bound",
    "sep_relu.relu_upper_bound",
    "sep_scale.scale",
    "sep_scale.scale_imag",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("dep_a")) {
    j.at("dep_a").get_to(*cask_plugin::toInternalShape(&p.dep_a));
  }
  if (j.contains("dep_b")) {
    j.at("dep_b").get_to(*cask_plugin::toInternalShape(&p.dep_b));
  }
  if (j.contains("sep_b")) {
    j.at("sep_b").get_to(*cask_plugin::toInternalShape(&p.sep_b));
  }
  if (j.contains("dep_bias")) {
    j.at("dep_bias").get_to(*cask_plugin::toInternalShape(&p.dep_bias));
  }
  if (j.contains("sep_bias")) {
    j.at("sep_bias").get_to(*cask_plugin::toInternalShape(&p.sep_bias));
  }
  if (j.contains("dep_alpha")) {
    j.at("dep_alpha").get_to(*cask_plugin::toInternalShape(&p.dep_alpha));
  }
  if (j.contains("sep_alpha")) {
    j.at("sep_alpha").get_to(*cask_plugin::toInternalShape(&p.sep_alpha));
  }
  if (j.contains("sep_c")) {
    j.at("sep_c").get_to(*cask_plugin::toInternalShape(&p.sep_c));
  }
  if (j.contains("dep_conv.groups")) { j.at("dep_conv.groups").get_to(p.dep_conv.groups); }
  if (j.contains("dep_conv.padding_front")) { j.at("dep_conv.padding_front").get_to(p.dep_conv.padding_front); }
  if (j.contains("dep_conv.padding_back")) { j.at("dep_conv.padding_back").get_to(p.dep_conv.padding_back); }
  if (j.contains("dep_conv.padding_top")) { j.at("dep_conv.padding_top").get_to(p.dep_conv.padding_top); }
  if (j.contains("dep_conv.padding_bottom")) { j.at("dep_conv.padding_bottom").get_to(p.dep_conv.padding_bottom); }
  if (j.contains("dep_conv.padding_left")) { j.at("dep_conv.padding_left").get_to(p.dep_conv.padding_left); }
  if (j.contains("dep_conv.padding_right")) { j.at("dep_conv.padding_right").get_to(p.dep_conv.padding_right); }
  if (j.contains("dep_conv.stride_depth")) { j.at("dep_conv.stride_depth").get_to(p.dep_conv.stride_depth); }
  if (j.contains("dep_conv.stride_height")) { j.at("dep_conv.stride_height").get_to(p.dep_conv.stride_height); }
  if (j.contains("dep_conv.stride_width")) { j.at("dep_conv.stride_width").get_to(p.dep_conv.stride_width); }
  if (j.contains("dep_conv.dilation_depth")) { j.at("dep_conv.dilation_depth").get_to(p.dep_conv.dilation_depth); }
  if (j.contains("dep_conv.dilation_height")) { j.at("dep_conv.dilation_height").get_to(p.dep_conv.dilation_height); }
  if (j.contains("dep_conv.dilation_width")) { j.at("dep_conv.dilation_width").get_to(p.dep_conv.dilation_width); }
  if (j.contains("dep_conv.is_xcorrelation")) { j.at("dep_conv.is_xcorrelation").get_to(p.dep_conv.is_xcorrelation); }
  if (j.contains("dep_with_relu")) { j.at("dep_with_relu").get_to(p.dep_with_relu); }
  if (j.contains("dep_relu.relu_lower_bound")) { j.at("dep_relu.relu_lower_bound").get_to(p.dep_relu.relu_lower_bound); }
  if (j.contains("dep_relu.relu_upper_bound")) { j.at("dep_relu.relu_upper_bound").get_to(p.dep_relu.relu_upper_bound); }
  if (j.contains("dep_scale.scale")) { j.at("dep_scale.scale").get_to(p.dep_scale.scale); }
  if (j.contains("dep_scale.scale_imag")) { j.at("dep_scale.scale_imag").get_to(p.dep_scale.scale_imag); }
  if (j.contains("sep_conv.groups")) { j.at("sep_conv.groups").get_to(p.sep_conv.groups); }
  if (j.contains("sep_conv.padding_front")) { j.at("sep_conv.padding_front").get_to(p.sep_conv.padding_front); }
  if (j.contains("sep_conv.padding_back")) { j.at("sep_conv.padding_back").get_to(p.sep_conv.padding_back); }
  if (j.contains("sep_conv.padding_top")) { j.at("sep_conv.padding_top").get_to(p.sep_conv.padding_top); }
  if (j.contains("sep_conv.padding_bottom")) { j.at("sep_conv.padding_bottom").get_to(p.sep_conv.padding_bottom); }
  if (j.contains("sep_conv.padding_left")) { j.at("sep_conv.padding_left").get_to(p.sep_conv.padding_left); }
  if (j.contains("sep_conv.padding_right")) { j.at("sep_conv.padding_right").get_to(p.sep_conv.padding_right); }
  if (j.contains("sep_conv.stride_depth")) { j.at("sep_conv.stride_depth").get_to(p.sep_conv.stride_depth); }
  if (j.contains("sep_conv.stride_height")) { j.at("sep_conv.stride_height").get_to(p.sep_conv.stride_height); }
  if (j.contains("sep_conv.stride_width")) { j.at("sep_conv.stride_width").get_to(p.sep_conv.stride_width); }
  if (j.contains("sep_conv.dilation_depth")) { j.at("sep_conv.dilation_depth").get_to(p.sep_conv.dilation_depth); }
  if (j.contains("sep_conv.dilation_height")) { j.at("sep_conv.dilation_height").get_to(p.sep_conv.dilation_height); }
  if (j.contains("sep_conv.dilation_width")) { j.at("sep_conv.dilation_width").get_to(p.sep_conv.dilation_width); }
  if (j.contains("sep_conv.is_xcorrelation")) { j.at("sep_conv.is_xcorrelation").get_to(p.sep_conv.is_xcorrelation); }
  if (j.contains("sep_with_relu")) { j.at("sep_with_relu").get_to(p.sep_with_relu); }
  if (j.contains("sep_relu.relu_lower_bound")) { j.at("sep_relu.relu_lower_bound").get_to(p.sep_relu.relu_lower_bound); }
  if (j.contains("sep_relu.relu_upper_bound")) { j.at("sep_relu.relu_upper_bound").get_to(p.sep_relu.relu_upper_bound); }
  if (j.contains("sep_scale.scale")) { j.at("sep_scale.scale").get_to(p.sep_scale.scale); }
  if (j.contains("sep_scale.scale_imag")) { j.at("sep_scale.scale_imag").get_to(p.sep_scale.scale_imag); }


}
template <typename JSON>
inline void to_json(JSON &j, const BnApplyFpropBnStat& op) {

  to_json(j["y"], *cask_plugin::toInternalShape(&op.y));

  to_json(j["y_data"], op.y_data);
  to_json(j["x"], *cask_plugin::toInternalShape(&op.x));

  to_json(j["x_data"], op.x_data);
  to_json(j["w"], *cask_plugin::toInternalShape(&op.w));

  to_json(j["w_data"], op.w_data);
  to_json(j["c"], *cask_plugin::toInternalShape(&op.c));

  to_json(j["c_data"], op.c_data);
  to_json(j["bn_res"], *cask_plugin::toInternalShape(&op.bn_res));

  to_json(j["bn_res_data"], op.bn_res_data);
  to_json(j["bn_res_add_relu_out"], *cask_plugin::toInternalShape(&op.bn_res_add_relu_out));

  to_json(j["bn_res_add_relu_out_data"], op.bn_res_add_relu_out_data);
  to_json(j["bn_scale"], *cask_plugin::toInternalShape(&op.bn_scale));

  to_json(j["bn_scale_data"], op.bn_scale_data);
  to_json(j["bn_bias"], *cask_plugin::toInternalShape(&op.bn_bias));

  to_json(j["bn_bias_data"], op.bn_bias_data);
  to_json(j["bn_res_scale"], *cask_plugin::toInternalShape(&op.bn_res_scale));

  to_json(j["bn_res_scale_data"], op.bn_res_scale_data);
  to_json(j["bn_res_bias"], *cask_plugin::toInternalShape(&op.bn_res_bias));

  to_json(j["bn_res_bias_data"], op.bn_res_bias_data);
  to_json(j["bn_mean"], *cask_plugin::toInternalShape(&op.bn_mean));

  to_json(j["bn_mean_data"], op.bn_mean_data);
  to_json(j["bn_inv_stddev"], *cask_plugin::toInternalShape(&op.bn_inv_stddev));

  to_json(j["bn_inv_stddev_data"], op.bn_inv_stddev_data);
  to_json(j["bn_sum"], *cask_plugin::toInternalShape(&op.bn_sum));

  to_json(j["bn_sum_data"], op.bn_sum_data);
  to_json(j["bn_sum_of_squares"], *cask_plugin::toInternalShape(&op.bn_sum_of_squares));

  to_json(j["bn_sum_of_squares_data"], op.bn_sum_of_squares_data);
  to_json(j["bn_bitmask_relu_out"], *cask_plugin::toInternalShape(&op.bn_bitmask_relu_out));

  to_json(j["bn_bitmask_relu_out_data"], op.bn_bitmask_relu_out_data);
  j["conv.groups"] = op.conv.groups;
  j["conv.padding_front"] = op.conv.padding_front;
  j["conv.padding_back"] = op.conv.padding_back;
  j["conv.padding_top"] = op.conv.padding_top;
  j["conv.padding_bottom"] = op.conv.padding_bottom;
  j["conv.padding_left"] = op.conv.padding_left;
  j["conv.padding_right"] = op.conv.padding_right;
  j["conv.stride_depth"] = op.conv.stride_depth;
  j["conv.stride_height"] = op.conv.stride_height;
  j["conv.stride_width"] = op.conv.stride_width;
  j["conv.dilation_depth"] = op.conv.dilation_depth;
  j["conv.dilation_height"] = op.conv.dilation_height;
  j["conv.dilation_width"] = op.conv.dilation_width;
  j["conv.is_xcorrelation"] = op.conv.is_xcorrelation;
  j["alpha"] = op.alpha;
  j["beta"] = op.beta;
  j["bn_epsilon"] = op.bn_epsilon;
}

template<typename JSON>
inline void from_json(const JSON& j, BnApplyFpropBnStat& p) {


  static std::set<std::string> allowed_keys = {
    "y",
    "x",
    "w",
    "c",
    "bn_res",
    "bn_res_add_relu_out",
    "bn_scale",
    "bn_bias",
    "bn_res_scale",
    "bn_res_bias",
    "bn_mean",
    "bn_inv_stddev",
    "bn_sum",
    "bn_sum_of_squares",
    "bn_bitmask_relu_out",
    "conv.groups",
    "conv.padding_front",
    "conv.padding_back",
    "conv.padding_top",
    "conv.padding_bottom",
    "conv.padding_left",
    "conv.padding_right",
    "conv.stride_depth",
    "conv.stride_height",
    "conv.stride_width",
    "conv.dilation_depth",
    "conv.dilation_height",
    "conv.dilation_width",
    "conv.is_xcorrelation",
    "alpha",
    "beta",
    "bn_epsilon",
  };

  check_json_keys(j, allowed_keys);

  // NOTE: Assume default value is already setup during construction of object
  if (j.contains("y")) {
    j.at("y").get_to(*cask_plugin::toInternalShape(&p.y));
  }
  if (j.contains("x")) {
    j.at("x").get_to(*cask_plugin::toInternalShape(&p.x));
  }
  if (j.contains("w")) {
    j.at("w").get_to(*cask_plugin::toInternalShape(&p.w));
  }
  if (j.contains("c")) {
    j.at("c").get_to(*cask_plugin::toInternalShape(&p.c));
  }
  if (j.contains("bn_res")) {
    j.at("bn_res").get_to(*cask_plugin::toInternalShape(&p.bn_res));
  }
  if (j.contains("bn_res_add_relu_out")) {
    j.at("bn_res_add_relu_out").get_to(*cask_plugin::toInternalShape(&p.bn_res_add_relu_out));
  }
  if (j.contains("bn_scale")) {
    j.at("bn_scale").get_to(*cask_plugin::toInternalShape(&p.bn_scale));
  }
  if (j.contains("bn_bias")) {
    j.at("bn_bias").get_to(*cask_plugin::toInternalShape(&p.bn_bias));
  }
  if (j.contains("bn_res_scale")) {
    j.at("bn_res_scale").get_to(*cask_plugin::toInternalShape(&p.bn_res_scale));
  }
  if (j.contains("bn_res_bias")) {
    j.at("bn_res_bias").get_to(*cask_plugin::toInternalShape(&p.bn_res_bias));
  }
  if (j.contains("bn_mean")) {
    j.at("bn_mean").get_to(*cask_plugin::toInternalShape(&p.bn_mean));
  }
  if (j.contains("bn_inv_stddev")) {
    j.at("bn_inv_stddev").get_to(*cask_plugin::toInternalShape(&p.bn_inv_stddev));
  }
  if (j.contains("bn_sum")) {
    j.at("bn_sum").get_to(*cask_plugin::toInternalShape(&p.bn_sum));
  }
  if (j.contains("bn_sum_of_squares")) {
    j.at("bn_sum_of_squares").get_to(*cask_plugin::toInternalShape(&p.bn_sum_of_squares));
  }
  if (j.contains("bn_bitmask_relu_out")) {
    j.at("bn_bitmask_relu_out").get_to(*cask_plugin::toInternalShape(&p.bn_bitmask_relu_out));
  }
  if (j.contains("conv.groups")) { j.at("conv.groups").get_to(p.conv.groups); }
  if (j.contains("conv.padding_front")) { j.at("conv.padding_front").get_to(p.conv.padding_front); }
  if (j.contains("conv.padding_back")) { j.at("conv.padding_back").get_to(p.conv.padding_back); }
  if (j.contains("conv.padding_top")) { j.at("conv.padding_top").get_to(p.conv.padding_top); }
  if (j.contains("conv.padding_bottom")) { j.at("conv.padding_bottom").get_to(p.conv.padding_bottom); }
  if (j.contains("conv.padding_left")) { j.at("conv.padding_left").get_to(p.conv.padding_left); }
  if (j.contains("conv.padding_right")) { j.at("conv.padding_right").get_to(p.conv.padding_right); }
  if (j.contains("conv.stride_depth")) { j.at("conv.stride_depth").get_to(p.conv.stride_depth); }
  if (j.contains("conv.stride_height")) { j.at("conv.stride_height").get_to(p.conv.stride_height); }
  if (j.contains("conv.stride_width")) { j.at("conv.stride_width").get_to(p.conv.stride_width); }
  if (j.contains("conv.dilation_depth")) { j.at("conv.dilation_depth").get_to(p.conv.dilation_depth); }
  if (j.contains("conv.dilation_height")) { j.at("conv.dilation_height").get_to(p.conv.dilation_height); }
  if (j.contains("conv.dilation_width")) { j.at("conv.dilation_width").get_to(p.conv.dilation_width); }
  if (j.contains("conv.is_xcorrelation")) { j.at("conv.is_xcorrelation").get_to(p.conv.is_xcorrelation); }
  if (j.contains("alpha")) { j.at("alpha").get_to(p.alpha); }
  if (j.contains("beta")) { j.at("beta").get_to(p.beta); }
  if (j.contains("bn_epsilon")) { j.at("bn_epsilon").get_to(p.bn_epsilon); }


}
}// namespace cask_plugin