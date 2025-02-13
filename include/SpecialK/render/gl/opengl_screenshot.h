#pragma once
/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#include <tuple>
#include <concurrent_vector.h>
#include <concurrent_queue.h>

#include <../depends/include/glm/glm.hpp>
#include <../depends/include/glm/detail/type_vec4.hpp>
#include <../depends/include/glm/gtc/packing.hpp>

#include <SpecialK/render/gl/opengl_backend.h>
#include <../depends/include/GL/glew.h>
#include <SpecialK/com_util.h>
#include <SpecialK/render/backend.h>
#include <SpecialK/render/screenshot.h>

class SK_GL_Screenshot
{
public:
  explicit SK_GL_Screenshot (SK_GL_Screenshot&& moveFrom) noexcept { *this = std::move (moveFrom); }
  explicit SK_GL_Screenshot (const HGLRC        hDevice,
                                   bool         allow_sound);

          ~SK_GL_Screenshot (void) {
            dispose ();
          }

  __inline bool isValid (void) noexcept { return hglrc              != nullptr &&
                                                 pixel_buffer_fence != nullptr; }
  __inline bool isReady (void) noexcept
  {
    if (                                      (! isValid ()) ||
       (ulCommandIssuedOnFrame > (SK_GetFramesDrawn () - 1))  )
    {
      return false;
    }

    if (pixel_buffer_fence == nullptr)
      return true; // We have no way of knowing

    GLsizei count  = 0;
    GLint   status = 0;

    glGetSynciv (pixel_buffer_fence, GL_SYNC_STATUS, 1, &count, &status);

    return
      (status == GL_SIGNALED);
  }

  SK_GL_Screenshot& __cdecl operator= (      SK_GL_Screenshot&& moveFrom);

  SK_GL_Screenshot                    (const SK_GL_Screenshot&          ) = delete;
  SK_GL_Screenshot&          operator=(const SK_GL_Screenshot&          ) = delete;

  void dispose (void);
  bool getData ( UINT* const pWidth,
                 UINT* const pHeight,
                 uint8_t   **ppData,
                 bool        Wait = false );

  __inline
  DXGI_FORMAT
  getInternalFormat (void) noexcept
  {
    return
      framebuffer.NativeFormat;
  }

  struct framebuffer_s
  {
    // One-time alloc, prevents allocating and freeing memory on the thread
    //   that memory-maps the GPU for perfect wait-free capture.
    struct PinnedBuffer {
      std::atomic_size_t           size    = 0L;
      std::unique_ptr <uint8_t []> bytes   = nullptr;
    } static root_;

    ~framebuffer_s (void)
    {
      if (PixelBuffer == root_.bytes)
        PixelBuffer.release (); // Does not free

      PixelBuffer.reset ();
    }

    UINT               Width               = 0,
                       Height              = 0;
    DXGI_FORMAT        NativeFormat        = DXGI_FORMAT_UNKNOWN;
    DXGI_ALPHA_MODE    AlphaMode           = DXGI_ALPHA_MODE_IGNORE;

    size_t             PBufferSize         = 0L;
    size_t             PackedDstPitch      = 0L,
                       PackedDstSlicePitch = 0L;

    std::unique_ptr
      <uint8_t []>     PixelBuffer         = nullptr;
  };

  __inline
  framebuffer_s*
  getFinishedData (void) noexcept
  {
    return
      ( framebuffer.PixelBuffer.get () != nullptr ) ?
       &framebuffer :                     nullptr;
  }

  __inline
    ULONG64
  getStartFrame (void) const noexcept
  {
    return
      ulCommandIssuedOnFrame;
  }

protected:
  HGLRC                           hglrc                  = nullptr;

  GLuint                          pixel_buffer_object    = 0;
  GLsync                          pixel_buffer_fence     = nullptr;
  ULONG64                         ulCommandIssuedOnFrame = 0;

  bool                            bPlaySound             =    true;
  framebuffer_s                   framebuffer            = {     };
};

void SK_GL_WaitOnAllScreenshots   (void);
void SK_GL_ProcessScreenshotQueue (SK_ScreenshotStage stage = SK_ScreenshotStage::EndOfFrame);
bool SK_Screenshot_GL_BeginFrame  (void);
void SK_Screenshot_GL_EndFrame    (void);
void SK_Screenshot_GL_RestoreHUD  (void);