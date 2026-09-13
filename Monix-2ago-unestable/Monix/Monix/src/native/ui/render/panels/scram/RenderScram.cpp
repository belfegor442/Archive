#include "ui/render/panels/scram/RenderScram.hpp"

void MonixApp::DrawScramView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  DrawPanel(dc, outer, L"SCRAM", ColorRole::Scram, smallFont_);
  DrawTextRect(dc, outer, L"...", ColorRole::Dim, bodyFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
