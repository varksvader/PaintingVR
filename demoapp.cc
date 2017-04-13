/*
 * Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vr/gvr/demos/controller_paint/jni/demoapp.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <string>

#include "vr/gvr/demos/controller_paint/jni/utils.h"

namespace {

// If true, requires the click button for painting. If false, the user can
// paint by simply touching the touchpad.
static const bool kRequireClickToPaint = true;

// Near and far clipping planes.
static const float kNearClip = 0.1f;
static const float kFarClip = 1000.0f;

// The distance at which we paint.
static const float kDefaultPaintDistance = 200.0f;

// File name for the paint texture. This is stored in the app's assets.
// The paint texture is stored in raw RGB format, with each three bytes
// encoding a pixel (the first byte is R, the second is G, the third is B).
static const char kPaintTexturePath[] = "paint_texture64x64.bin";
static const char kGroundTexturePath[] = "ground_texture64x64.bin";

// Width and height of the paint texture, in pixels.
static const int kPaintTextureWidth = 64;
static const int kPaintTextureHeight = 64;
static const int kGroundTextureWidth = 64;
static const int kGroundTextureHeight = 64;

// Colors (R, G, B).
static const std::array<float, 4> kSkyColor = Utils::ColorFromHex(0xff131e35);
static const std::array<float, 4> kGroundColor =
    Utils::ColorFromHex(0xff172644);
static const std::array<float, 4> kCursorBorderColor =
    { 1.0f, 1.0f, 1.0f, 1.0f };

// Vertex shader.
static const char* kPaintShaderVp =
    "uniform mat4 u_MVP;\n"
    "attribute vec4 a_Position;\n"
    "attribute vec2 a_TexCoords;\n"
    "varying vec2 v_TexCoords;\n"
    "void main() {\n"
    "  gl_Position = u_MVP * a_Position;\n"
    "  v_TexCoords = a_TexCoords;\n"
    "}\n";

// Fragment shader.
static const char* kPaintShaderFp =
    "precision mediump float;\n"
    "uniform vec4 u_Color;\n"
    "varying vec2 v_TexCoords;\n"
    "uniform sampler2D u_Sampler;\n"
    "void main() {\n"
    "  gl_FragColor = u_Color * texture2D(u_Sampler, vec2(\n"
    "      fract(v_TexCoords.s), fract(v_TexCoords.t)));\n"
    "}\n";

// In geometry data, this is the offset where texture coordinates start.
static int kGeomTexCoordOffset = 3;  // in elements, not bytes.

// In geometry data, this is how many bytes we skip ahead to get to the
// data about the next vertex (3 floats for vertex coords, 2 for texture
// coords).
static int kGeomDataStride = 5 * sizeof(float);

// Repetitions of the ground texture.
static float kGroundTexRepeat = 200.0f;

// Size of the ground plane.
static float kGroundSize = 300.0f;

// Y coordinate of the ground plane.
static float kGroundY = -20.0f;

// Ground's model matrix.
static const gvr::Mat4f kGroundModelMatrix = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, kGroundY,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

// Geometry of the ground plane.
static float kGroundGeom[] = {
    // Data is X, Y, Z (vertex coords), S, T (texture coords).
    kGroundSize, 0.0f, -kGroundSize, kGroundTexRepeat, 0.0f,
    -kGroundSize, 0.0f, -kGroundSize, 0.0f, 0.0f,
    -kGroundSize, 0.0f, kGroundSize, 0.0f, kGroundTexRepeat,
    kGroundSize, 0.0f, -kGroundSize, kGroundTexRepeat, 0.0f,
    -kGroundSize, 0.0f, kGroundSize, 0.0f, kGroundTexRepeat,
    kGroundSize, 0.0f, kGroundSize, kGroundTexRepeat, kGroundTexRepeat,
};
static int kGroundVertexCount = 6;

// Size of the cursor.
static float kCursorScale = 1.0f;

// Geometry of the cursor.
static float kCursorGeom[] = {
    // Data is X, Y, Z (vertex coords), S, T (texture coords).
    kCursorScale, kCursorScale, 0.0f, 1.0f, 0.0f,
    -kCursorScale, kCursorScale, 0.0f, 0.0f, 0.0f,
    -kCursorScale, -kCursorScale, 0.0f, 0.0f, 1.0f,
    kCursorScale, kCursorScale, 0.0f, 1.0f, 0.0f,
    -kCursorScale, -kCursorScale, 0.0f, 0.0f, 1.0f,
    kCursorScale, -kCursorScale, 0.0f, 1.0f, 1.0f,
};
static int kCursorVertexCount = 6;

// Available colors the user can paint with.
static const std::array<std::array<float, 4>, 82> kColors = {
    Utils::ColorFromHex(0xa0fff0e6),  // lightest orange
    Utils::ColorFromHex(0xa0ffd1b3),  // lighter orange
    Utils::ColorFromHex(0xa0ffb380),  // light orange
    Utils::ColorFromHex(0xa0ff944d),  // kinda light orange
    Utils::ColorFromHex(0xa0ff751a),  // lightish orange
    Utils::ColorFromHex(0xa0e65c00),  // darkish orange
    Utils::ColorFromHex(0xa0b34700),  // kinda dark orange
    Utils::ColorFromHex(0xa0803300),  // dark orange
    Utils::ColorFromHex(0xa04d1f00),  // darker orange
    Utils::ColorFromHex(0xa01a0a00),  // darkest orange
    Utils::ColorFromHex(0xa0845f00),  // brown
    Utils::ColorFromHex(0xa01a0000),  // darkest red
    Utils::ColorFromHex(0xa04d0000),  // darker red
    Utils::ColorFromHex(0xa0800000),  // dark red
    Utils::ColorFromHex(0xa0b30000),  // kinda dark red
    Utils::ColorFromHex(0xa0e60000),  // darkish red
    Utils::ColorFromHex(0xa0ff1a1a),  // lightish red
    Utils::ColorFromHex(0xa0ff4d4d),  // kinda light red
    Utils::ColorFromHex(0xa0ff8080),  // light red
    Utils::ColorFromHex(0xa0ffb3b3),  // lighter red
    Utils::ColorFromHex(0xa0ffe6e6),  // lightest red
    Utils::ColorFromHex(0xa0ffe6ff),  // lightest pink
    Utils::ColorFromHex(0xa0ffb3ff),  // lighter pink
    Utils::ColorFromHex(0xa0ff80ff),  // light pink
    Utils::ColorFromHex(0xa0ff4dff),  // kinda light pink
    Utils::ColorFromHex(0xa0ff1aff),  // lightish pink
    Utils::ColorFromHex(0xa0e600e6),  // darkish pink
    Utils::ColorFromHex(0xa0b300b3),  // kinda dark pink
    Utils::ColorFromHex(0xa0800080),  // dark pink
    Utils::ColorFromHex(0xa04d004d),  // darker pink
    Utils::ColorFromHex(0xa01a001a),  // darkest pink
    Utils::ColorFromHex(0xa013001a),  // darkest purple
    Utils::ColorFromHex(0xa039004d),  // darker purple
    Utils::ColorFromHex(0xa0600080),  // dark purple
    Utils::ColorFromHex(0xa08600b3),  // kinda dark purple
    Utils::ColorFromHex(0xa0ac00e6),  // darkish purple
    Utils::ColorFromHex(0xa0c61aff),  // lightish purple
    Utils::ColorFromHex(0xa0d24dff),  // kinda light purple
    Utils::ColorFromHex(0xa0df80ff),  // light purple
    Utils::ColorFromHex(0xa0ecb3ff),  // lighter purple
    Utils::ColorFromHex(0xa0f9e6ff),  // lightest purple
    Utils::ColorFromHex(0xa0e6ecff),  // lightest blue
    Utils::ColorFromHex(0xa0b3c6ff),  // lighter blue
    Utils::ColorFromHex(0xa0809fff),  // light blue
    Utils::ColorFromHex(0xa04d79ff),  // kinda light blue
    Utils::ColorFromHex(0xa01a1aff),  // lightish blue
    Utils::ColorFromHex(0xa00000b3),  // darkish blue
    Utils::ColorFromHex(0xa0000080),  // kinda dark blue
    Utils::ColorFromHex(0xa000001a),  // darker blue
    Utils::ColorFromHex(0xa000004d),  // dark blue
    Utils::ColorFromHex(0xa000001a),  // darkest blue
    Utils::ColorFromHex(0xa0000000),  // black
    Utils::ColorFromHex(0xa01a1a1a),  // darkest gray
    Utils::ColorFromHex(0xa0333333),  // darker gray
    Utils::ColorFromHex(0xa04d4d4d),  // dark gray
    Utils::ColorFromHex(0xa0666666),  // kinda dark gray
    Utils::ColorFromHex(0xa0808080),  // darkish gray
    Utils::ColorFromHex(0xa0999999),  // lightish gray
    Utils::ColorFromHex(0xa0b3b3b3),  // kinda light gray
    Utils::ColorFromHex(0xa0cccccc),  // light gray
    Utils::ColorFromHex(0xa0e6e6e6),  // lighter gray
    Utils::ColorFromHex(0xa0ffffff),  // lightest gray
    Utils::ColorFromHex(0xa0e6ffee),  // lightest green
    Utils::ColorFromHex(0xa0b3ffcc),  // lighter green
    Utils::ColorFromHex(0xa080ffaa),  // light green
    Utils::ColorFromHex(0xa04dff88),  // kinda light green
    Utils::ColorFromHex(0xa01aff66),  // lightish green
    Utils::ColorFromHex(0xa000e64d),  // darkish green
    Utils::ColorFromHex(0xa000b33c),  // kinda dark green
    Utils::ColorFromHex(0xa000802b),  // dark green
    Utils::ColorFromHex(0xa0004d1a),  // darker green
    Utils::ColorFromHex(0xa0001a09),  // darkest green
    Utils::ColorFromHex(0xa01a1a00),  // darkest yellow
    Utils::ColorFromHex(0xa04d4d00),  // darker yellow
    Utils::ColorFromHex(0xa0808000),  // dark yellow
    Utils::ColorFromHex(0xa0b3b300),  // kinda dark yellow
    Utils::ColorFromHex(0xa0e6e600),  // darkish yellow
    Utils::ColorFromHex(0xa0ffff1a),  // lightish yellow
    Utils::ColorFromHex(0xa0ffff4d),  // kinda light yellow
    Utils::ColorFromHex(0xa0ffff80),  // light yellow
    Utils::ColorFromHex(0xa0ffffb3),  // lighter yellow
    Utils::ColorFromHex(0xa0ffffe6),  // lightest yellow
    };

// When the user moves their finger horizontally by more than this
// threshold distance, we switch colors.
// This is given as a fraction of the touch pad.
static const float kColorSwitchThreshold = 0.4f;

// Maximum number of drawn vertices to allow a color switch.
// If more than this number of vertices have been drawn, then a color
// switch gesture is forbidden.
static const int kMaxVerticesForColorSwitch = 10;

// Prediction time to use when estimating head pose.
static const int64_t kPredictionTimeWithoutVsyncNanos = 50000000;  // 50ms

// Minimum length of any paint segment. If the user tries to draw something
// smaller than this length, it is ignored.
static const float kMinPaintSegmentLength = 4.0f;

// When the number of vertices in the recently drawn geometry exceeds this
// number, we commit the geometry to the GPU as a VBO.
static const int kVboCommitThreshold = 50;

// Minimum and maximum stroke widths.
static const float kMinStrokeWidth = 1.5f;
static const float kMaxStrokeWidth = 4.0f;

}  // namespace

DemoApp::DemoApp(JNIEnv* env, jobject asset_mgr_obj, jlong gvr_context_ptr)
    :  // This is the GVR context pointer obtained from Java:
      gvr_context_(reinterpret_cast<gvr_context*>(gvr_context_ptr)),
      // Wrap the gvr_context* into a GvrApi C++ object for convenience:
      gvr_api_(gvr::GvrApi::WrapNonOwned(gvr_context_)),
      gvr_api_initialized_(false),
      viewport_list_(gvr_api_->CreateEmptyBufferViewportList()),
      scratch_viewport_(gvr_api_->CreateBufferViewport()),
      shader_(-1),
      shader_u_color_(-1),
      shader_u_mvp_matrix_(-1),
      shader_u_sampler_(-1),
      shader_a_position_(-1),
      shader_a_texcoords_(-1),
      ground_texture_(-1),
      paint_texture_(-1),
      asset_mgr_(AAssetManager_fromJava(env, asset_mgr_obj)),
      recent_geom_vertex_count_(0),
      brush_stroke_total_vertices_(0),
      selected_color_(0),
      painting_(false),
      has_continuation_(false),
      switched_color_(false),
      stroke_width_(kMinStrokeWidth) {
  CHECK(asset_mgr_);
  LOGD("DemoApp initialized.");
}

DemoApp::~DemoApp() {
  LOGD("DemoApp shutdown.");
}

void DemoApp::OnResume() {
  LOGD("DemoApp::OnResume");
  if (gvr_api_initialized_) {
    gvr_api_->RefreshViewerProfile();
    gvr_api_->ResumeTracking();
  }
  if (controller_api_) controller_api_->Resume();
}

void DemoApp::OnPause() {
  LOGD("DemoApp::OnPause");
  if (gvr_api_initialized_) gvr_api_->PauseTracking();
  if (controller_api_) controller_api_->Pause();
}

void DemoApp::OnSurfaceCreated() {
  LOGD("DemoApp::OnSurfaceCreated");

  LOGD("Initializing GL on GvrApi.");
  gvr_api_->InitializeGl();

  LOGD("Initializing ControllerApi.");
  controller_api_.reset(new gvr::ControllerApi);
  CHECK(controller_api_);
  CHECK(controller_api_->Init(gvr::ControllerApi::DefaultOptions(),
                              gvr_context_));
  controller_api_->Resume();

  LOGD("Initializing framebuffer.");
  std::vector<gvr::BufferSpec> specs;
  specs.push_back(gvr_api_->CreateBufferSpec());
  framebuf_size_ = gvr_api_->GetMaximumEffectiveRenderTargetSize();
  // Because we are using 2X MSAA, we can render to half as many pixels and
  // achieve similar quality. Scale each dimension by sqrt(2)/2 ~= 7/10ths.
  framebuf_size_.width = (7 * framebuf_size_.width) / 10;
  framebuf_size_.height = (7 * framebuf_size_.height) / 10;

  specs[0].SetSize(framebuf_size_);
  specs[0].SetColorFormat(GVR_COLOR_FORMAT_RGBA_8888);
  specs[0].SetDepthStencilFormat(GVR_DEPTH_STENCIL_FORMAT_DEPTH_16);
  specs[0].SetSamples(2);
  swapchain_.reset(new gvr::SwapChain(gvr_api_->CreateSwapChain(specs)));

  LOGD("Compiling shaders.");
  int vp = Utils::BuildShader(GL_VERTEX_SHADER, kPaintShaderVp);
  int fp = Utils::BuildShader(GL_FRAGMENT_SHADER, kPaintShaderFp);
  shader_ = Utils::BuildProgram(vp, fp);
  shader_u_color_ = glGetUniformLocation(shader_, "u_Color");
  shader_u_mvp_matrix_ = glGetUniformLocation(shader_, "u_MVP");
  shader_u_sampler_ = glGetUniformLocation(shader_, "u_Sampler");
  shader_a_position_ = glGetAttribLocation(shader_, "a_Position");
  shader_a_texcoords_ = glGetAttribLocation(shader_, "a_TexCoords");
  CHECK(glGetError() == GL_NO_ERROR);

  LOGD("Loading textures.");
  paint_texture_ = Utils::LoadRawTextureFromAsset(
      asset_mgr_, kPaintTexturePath, kPaintTextureWidth, kPaintTextureHeight);
  ground_texture_ = Utils::LoadRawTextureFromAsset(
      asset_mgr_, kGroundTexturePath, kGroundTextureWidth,
      kGroundTextureHeight);

  CHECK(glGetError() == GL_NO_ERROR);
  gvr_api_initialized_ = true;


  LOGD("Init complete.");
}

void DemoApp::OnSurfaceChanged(int width, int height) {
  LOGD("DemoApp::OnSurfaceChanged %dx%d", width, height);
}

void DemoApp::OnDrawFrame() {
  PrepareFramebuffer();

  // Enable blending so we get a transparency effect.
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  viewport_list_.SetToRecommendedBufferViewports();
  gvr::ClockTimePoint pred_time = gvr::GvrApi::GetTimePointNow();
  pred_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_view =
      gvr_api_->GetHeadSpaceFromStartSpaceRotation(pred_time);
  const gvr::Mat4f left_eye_view =
      Utils::MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_LEFT_EYE),
                       head_view);
  const gvr::Mat4f right_eye_view =
      Utils::MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_RIGHT_EYE),
                       head_view);

  const int32_t old_status = controller_state_.GetApiStatus();
  const int32_t old_connection_state = controller_state_.GetConnectionState();

  // Read current controller state.
  controller_state_.Update(*controller_api_);

  // Print new API status and connection state, if they changed.
  if (controller_state_.GetApiStatus() != old_status ||
      controller_state_.GetConnectionState() != old_connection_state) {
    LOGD("DemoApp: controller API status: %s, connection state: %s",
         gvr_controller_api_status_to_string(controller_state_.GetApiStatus()),
         gvr_controller_connection_state_to_string(
             controller_state_.GetConnectionState()));
  }

  gvr::Frame frame = swapchain_->AcquireFrame();
  frame.BindBuffer(0);

  glClearColor(kSkyColor[0], kSkyColor[1], kSkyColor[2], 1.0f);
  viewport_list_.GetBufferViewport(0, &scratch_viewport_);
  DrawEye(GVR_LEFT_EYE, left_eye_view, scratch_viewport_);
  viewport_list_.GetBufferViewport(1, &scratch_viewport_);
  DrawEye(GVR_RIGHT_EYE, right_eye_view, scratch_viewport_);
  frame.Unbind();
  frame.Submit(viewport_list_, head_view);
}

void DemoApp::PrepareFramebuffer() {
  gvr::Sizei recommended_size = gvr_api_->GetMaximumEffectiveRenderTargetSize();
  // Because we are using 2X MSAA, we can render to half as many pixels and
  // achieve similar quality. Scale each dimension by sqrt(2)/2 ~= 7/10ths.
  recommended_size.width = (7 * recommended_size.width) / 10;
  recommended_size.height = (7 * recommended_size.height) / 10;
  if (framebuf_size_.width != recommended_size.width ||
      framebuf_size_.height != recommended_size.height) {
    // We need to resize the framebuffer.
    swapchain_->ResizeBuffer(0, recommended_size);
    framebuf_size_ = recommended_size;
  }
}

void DemoApp::CheckColorSwitch() {
  if (switched_color_ || !controller_state_.IsTouching()) return;
  float x_diff = fabs(controller_state_.GetTouchPos().x - touch_down_x_);
  if (x_diff < kColorSwitchThreshold) return;
  if (brush_stroke_total_vertices_ > kMaxVerticesForColorSwitch) return;
  if (controller_state_.GetTouchPos().x > touch_down_x_) {
    selected_color_ = (selected_color_ + 1) % kColors.size();
  } else {
    selected_color_ = selected_color_ == 0 ? kColors.size() - 1 :
        selected_color_ - 1;
  }
  switched_color_ = true;
}

void DemoApp::CheckChangeStrokeWidth() {
  if (!controller_state_.IsTouching()) return;
  float delta_y = controller_state_.GetTouchPos().y - touch_down_y_;
  float delta_width = -delta_y * (kMaxStrokeWidth - kMinStrokeWidth);
  stroke_width_ = touch_down_stroke_width_ + delta_width;
  stroke_width_ = stroke_width_ < kMinStrokeWidth ? kMinStrokeWidth :
      stroke_width_ > kMaxStrokeWidth ? kMaxStrokeWidth : stroke_width_;
}

void DemoApp::DrawEye(gvr::Eye which_eye, const gvr::Mat4f& eye_view_matrix,
                      const gvr::BufferViewport& viewport) {
  Utils::SetUpViewportAndScissor(framebuf_size_, viewport);

  gvr::Mat4f proj_matrix =
      Utils::PerspectiveMatrixFromView(viewport.GetSourceFov(), kNearClip,
                                       kFarClip);

  // Figure out the point the cursor is pointing to.
  const gvr::Mat4f cursor_mat =
      Utils::ControllerQuatToMatrix(controller_state_.GetOrientation());
  const std::array<float, 3> neutral_pos = { 0, 0, -kDefaultPaintDistance };
  const std::array<float, 3> target_pos = Utils::MatrixVectorMul(
      cursor_mat, neutral_pos);

  bool paint_button_down =
      kRequireClickToPaint
          ? controller_state_.GetButtonDown(gvr::kControllerButtonClick)
          : controller_state_.GetTouchDown();
  bool paint_button_up =
      kRequireClickToPaint
          ? controller_state_.GetButtonUp(gvr::kControllerButtonClick)
          : controller_state_.GetTouchUp();

  if (paint_button_down) {
    StartPainting(target_pos);
  } else if (paint_button_up) {
    StopPainting(true);
  }

  if (controller_state_.GetTouchDown()) {
    touch_down_x_ = controller_state_.GetTouchPos().x;
    touch_down_y_ = controller_state_.GetTouchPos().y;
    touch_down_stroke_width_ = stroke_width_;
  } else if (controller_state_.GetTouchUp()) {
    switched_color_ = false;
  }

  if (!painting_ &&
      controller_state_.GetButtonDown(gvr::kControllerButtonApp)) {
    ClearDrawing();
  }

  CheckColorSwitch();
  CheckChangeStrokeWidth();

  if (painting_) {
    const float dist = Utils::VecNorm(
        Utils::VecAdd(1, paint_anchor_, -1, target_pos));
    if (dist > kMinPaintSegmentLength) {
      AddPaintSegment(paint_anchor_, target_pos);
      paint_anchor_ = target_pos;
    }
  }

  glUseProgram(shader_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ground_texture_);
  DrawGround(eye_view_matrix, proj_matrix);
  glBindTexture(GL_TEXTURE_2D, paint_texture_);
  DrawPaintedGeometry(eye_view_matrix, proj_matrix);
  DrawCursor(eye_view_matrix, proj_matrix);

  CHECK(glGetError() == GL_NO_ERROR);
}

void DemoApp::AddVertex(const std::array<float, 3>& coords, float u, float v) {
  ++recent_geom_vertex_count_;
  ++brush_stroke_total_vertices_;
  recent_geom_.push_back(coords[0]);
  recent_geom_.push_back(coords[1]);
  recent_geom_.push_back(coords[2]);
  recent_geom_.push_back(u);
  recent_geom_.push_back(v);
}

void DemoApp::AddPaintSegment(const std::array<float, 3>& start_point,
                              const std::array<float, 3>& end_point) {
  std::array<float, 3> to_end = Utils::VecAdd(1, end_point, -1, start_point);
  std::array<float, 3> cross = Utils::VecNormalize(
      Utils::VecCrossProd(start_point, to_end));

  std::array<float, 3> start_top;
  std::array<float, 3> start_bottom;

  if (has_continuation_) {
    // Continue from where we left off to form a continuous shape.
    start_top = continuation_points_[0];
    start_bottom = continuation_points_[1];
  } else {
    // Start of a new shape.
    start_top = Utils::VecAdd(1, start_point, stroke_width_, cross);
    start_bottom = Utils::VecAdd(1, start_point, -stroke_width_, cross);
  }

  const std::array<float, 3> end_top = Utils::VecAdd(
      1, end_point, stroke_width_, cross);
  const std::array<float, 3> end_bottom = Utils::VecAdd(
      1, end_point, -stroke_width_, cross);
  AddVertex(start_top, 0.0f, 0.0f);
  AddVertex(start_bottom, 0.0f, 1.0f);
  AddVertex(end_top, 1.0f, 0.0f);
  AddVertex(start_bottom, 0.0f, 1.0f);
  AddVertex(end_bottom, 1.0f, 1.0f);
  AddVertex(end_top, 1.0f, 0.0f);
  if (recent_geom_vertex_count_ > kVboCommitThreshold) {
    CommitToVbo();
  }

  has_continuation_ = true;
  continuation_points_[0] = end_top;
  continuation_points_[1] = end_bottom;
}

void DemoApp::StartPainting(const std::array<float, 3> paint_start_pos) {
  if (painting_) return;
  painting_ = true;
  paint_anchor_ = paint_start_pos;
}

void DemoApp::StopPainting(bool commit_cur_segment) {
  if (!painting_) return;
  if (commit_cur_segment) {
    CommitToVbo();
  }
  recent_geom_.clear();
  recent_geom_vertex_count_ = 0;
  painting_ = false;
  has_continuation_ = false;
  brush_stroke_total_vertices_ = 0;
}

void DemoApp::ClearDrawing() {
  for (auto it : committed_vbos_) {
    glDeleteBuffers(1, &it.vbo);
  }
  committed_vbos_.clear();
}

void DemoApp::DrawObject(const gvr::Mat4f& mvp,
                         const std::array<float, 4>& color, const float* data,
                         GLuint vbo, int vertex_count) {
  if (!data) {
    // Use VBO.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
  }

  glUniform1i(shader_u_sampler_, 0);  // texture unit 0
  glUniformMatrix4fv(shader_u_mvp_matrix_, 1, GL_FALSE,
                     Utils::MatrixToGLArray(mvp).data());
  glUniform4f(shader_u_color_, color[0], color[1], color[2], color[3]);
  glEnableVertexAttribArray(shader_a_position_);
  glEnableVertexAttribArray(shader_a_texcoords_);
  glVertexAttribPointer(shader_a_position_, 3, GL_FLOAT, false,
                        kGeomDataStride, data);
  glVertexAttribPointer(shader_a_texcoords_, 2, GL_FLOAT, false,
                        kGeomDataStride, data + kGeomTexCoordOffset);
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);

  if (!data) {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

void DemoApp::DrawGround(const gvr::Mat4f& view_matrix,
                         const gvr::Mat4f& proj_matrix) {
  gvr::Mat4f mv = Utils::MatrixMul(view_matrix, kGroundModelMatrix);
  gvr::Mat4f mvp = Utils::MatrixMul(proj_matrix, mv);

  DrawObject(mvp, kGroundColor, kGroundGeom, 0, kGroundVertexCount);
}

void DemoApp::DrawPaintedGeometry(const gvr::Mat4f& view_matrix,
                                  const gvr::Mat4f& proj_matrix) {
  gvr::Mat4f mv = view_matrix;
  gvr::Mat4f mvp = Utils::MatrixMul(proj_matrix, mv);

  // Draw committed VBOs.
  for (auto it : committed_vbos_) {
    DrawObject(mvp, kColors[it.color], 0, it.vbo, it.vertex_count);
  }

  // Draw recent geometry (directly from main memory).
  DrawObject(mvp, kColors[selected_color_], recent_geom_.data(), 0,
             recent_geom_vertex_count_);
}

void DemoApp::CommitToVbo() {
  // Only commit if we have at least a triangle.
  if (recent_geom_vertex_count_ > 2) {
    VboInfo info;
    glGenBuffers(1, &info.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, info.vbo);
    glBufferData(GL_ARRAY_BUFFER, recent_geom_.size() * sizeof(float),
                 recent_geom_.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    info.vertex_count = recent_geom_vertex_count_;
    info.color = selected_color_;
    committed_vbos_.push_back(info);
  }
  recent_geom_.clear();
  recent_geom_vertex_count_ = 0;
}

void DemoApp::DrawCursorRect(float scale, const std::array<float, 4> color,
                             const gvr::Mat4f& view_matrix,
                             const gvr::Mat4f& proj_matrix) {
  gvr::Mat4f neutral_matrix = {
      scale, 0.0f, 0.0f, 0.0f, 0.0f,  scale,
      0.0f,  0.0f, 0.0f, 0.0f, scale, -kDefaultPaintDistance,
      0.0f,  0.0f, 0.0f, 1.0f,
  };
  gvr::Mat4f controller_matrix =
      Utils::ControllerQuatToMatrix(controller_state_.GetOrientation());
  gvr::Mat4f model_matrix = Utils::MatrixMul(controller_matrix, neutral_matrix);
  gvr::Mat4f mv = Utils::MatrixMul(view_matrix, model_matrix);
  gvr::Mat4f mvp = Utils::MatrixMul(proj_matrix, mv);
  DrawObject(mvp, color, kCursorGeom, 0, kCursorVertexCount);
}

void DemoApp::DrawCursor(const gvr::Mat4f& view_matrix,
                         const gvr::Mat4f& proj_matrix) {
  float scale = stroke_width_ / kMinStrokeWidth;
  DrawCursorRect(scale * 1.50f, kCursorBorderColor, view_matrix, proj_matrix);
  DrawCursorRect(scale * 1.25f, { 0.0f, 0.0f, 0.0f, 1.0f }, view_matrix,
                 proj_matrix);
  DrawCursorRect(scale * 1.00f, kColors[selected_color_], view_matrix,
                 proj_matrix);
}

