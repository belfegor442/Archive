#pragma once

namespace monix {

struct RegistryState {
  struct Hive {
    int keyCount = 0;
    int valueCount = 0;
    unsigned long hash = 0;
  };

  Hive run, runOnce, shell, policies, services, drivers;
  Hive firewall, uac, taskSched, com, telemetry, audit;
  Hive env, path, appAssoc, shellExt, defApp, fileAssoc, configFile;
  int startupFolderCount = 0;
  int envVarCount = 0;
  unsigned long regHashEnvPath = 0;
};

}
