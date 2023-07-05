/*
Copyright 2017-2020 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <SpecialK/render/present_mon/PresentMon.hpp>
#include <SpecialK/utility.h>
#include <imgui/font_awesome.h>

extern char          SK_PresentDebugStr [2][256];
extern volatile LONG SK_PresentIdx;

#include <SpecialK/render/backend.h>

void
UpdateConsole ( uint32_t           processId,
                ProcessInfo const& processInfo )
{
  UNREFERENCED_PARAMETER (processId);

  // Don't display non-target or empty processes
  if ((! processInfo.mTargetProcess)      ||
         processInfo.mSwapChain.empty  ())
  {
    return;
  }

  auto empty = true;

  static char szRuntime [16] = { };

  for ( auto const& pair : processInfo.mSwapChain )
  {
  //auto        address = pair.first;
    auto const& chain   = pair.second;

    // Only show swapchain data if there at least two presents in the
    // history.
    if (chain.mPresentHistoryCount < 2)
      continue;

    //auto const& present0 =
    //  *chain.mPresentHistory [
    //  (chain.mNextPresentIndex - chain.mPresentHistoryCount) %
    //                         SwapChainData::PRESENT_HISTORY_MAX_COUNT
    //                         ];
    //auto const& presentN =
    //  *chain.mPresentHistory [
    //  (chain.mNextPresentIndex - 1)                          %
    //                         SwapChainData::PRESENT_HISTORY_MAX_COUNT
    //                         ];

  //auto cpuAvg =
  //  QpcDeltaToSeconds (presentN.QpcTime - present0.QpcTime) /
  //                           (chain.mPresentHistoryCount - 1);
    auto dspAvg  = 0.0;
    auto latAvg  = 0.0;
    auto idleAvg = 0.0;

    PresentEvent* displayN = nullptr;

    ///if (args.mTrackDisplay)
    ///if (true)
    {
      uint64_t display0ScreenTime = 0;
      uint64_t latSum             = 0;
      uint64_t idleSum            = 0;
      uint32_t displayCount       = 0;

      for ( uint32_t i = 0                          ;
                     i < chain.mPresentHistoryCount ;
                   ++i )
      {
        auto const& p =
          chain.mPresentHistory [
            (chain.mNextPresentIndex - chain.mPresentHistoryCount + i) %
                                   SwapChainData::PRESENT_HISTORY_MAX_COUNT
                                ];

        if (p->FinalState == PresentResult::Presented)
        {
          if (displayCount == 0)
          {
            display0ScreenTime = p->ScreenTime;
          }

          displayN  = p.get ();
           latSum  += p->ScreenTime - p->QpcTime;
           idleSum += p->ScreenTime - p->ReadyTime;

          displayCount++;
        }
      }

      if (displayCount >= 2)
      {
        dspAvg = QpcDeltaToSeconds (displayN->ScreenTime - display0ScreenTime) /
                                                            (displayCount - 1);
      }

      if (displayCount >= 1)
      {
        latAvg  = QpcDeltaToSeconds (latSum)                / displayCount;
        idleAvg = QpcDeltaToSeconds (idleSum)               / displayCount;
      }

      if (displayCount >  0)
        std::exchange (empty, false);
    }

    int idx =
      (ReadAcquire (&SK_PresentIdx) + 1) % 2;

    std::string_view
      present_debug_view (
        SK_PresentDebugStr [idx], 256);

    static auto& rb =
      SK_GetCurrentRenderBackend ();

    std::chrono::time_point <std::chrono::system_clock> now =
      std::chrono::system_clock::now ();

    static constexpr char* pszHourglasses [] = {
      ICON_FA_HOURGLASS_START,
      ICON_FA_HOURGLASS_HALF,
      ICON_FA_HOURGLASS_END,
      ICON_FA_HOURGLASS_HALF,
    };

    const char* szHourglass =
      pszHourglasses [
        (std::chrono::duration_cast <std::chrono::milliseconds> (
          now.time_since_epoch ()).count () % 4000) / 1000
      ];

    if (displayN != nullptr)
    {
      const bool fast_path =
        ( displayN->PresentMode == PresentMode::Hardware_Legacy_Flip                 ) ||
        ( displayN->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer ) ||
        ( displayN->PresentMode == PresentMode::Hardware_Independent_Flip            ) ||
        ( displayN->PresentMode == PresentMode::Hardware_Composed_Independent_Flip   );

      rb.present_mode = displayN->PresentMode;

      SK_FormatStringView (
          present_debug_view,
            " " ICON_FA_LINK "  %8ws    %s%s    ",
              rb.name, fast_path ? ICON_FA_TACHOMETER_ALT       " "
                                 : ICON_FA_EXCLAMATION_TRIANGLE " ",
                PresentModeToString (displayN->PresentMode)
                          );

      SK_FormatStringView (
          present_debug_view, "%hs%s %5.2f ms    ",
          present_debug_view.data (),  szHourglass,
                latAvg * 1000.0
                          );

      extern float
          SK_Framerate_GetBusyWaitPercent (void);
      if (SK_Framerate_GetBusyWaitPercent () > 0.0 && SK_Framerate_GetBusyWaitPercent () <= 100.0)
      {
        SK_FormatStringView (
          present_debug_view, "%hs" //ICON_FA_STOPWATCH " %5.2f ms",
                                      ICON_FA_MICROCHIP " %3.1f%%",
          present_debug_view.data (),
                SK_Framerate_GetBusyWaitPercent ()//idleAvg * 1000.0//1.0 / dspAvg
                            );
      }
    }
  }

  if (! empty)
  {
    WriteRelease (&SK_PresentIdx, (
     ReadAcquire (&SK_PresentIdx) + 1
                                  ) % 2);
  }
}

