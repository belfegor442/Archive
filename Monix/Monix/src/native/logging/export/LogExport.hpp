#pragma once

class MonixApp;

namespace monix::export_ {

void ExportLogsJson(const MonixApp* app);
void ExportLogsCsv(const MonixApp* app);

}  // namespace monix::export_
