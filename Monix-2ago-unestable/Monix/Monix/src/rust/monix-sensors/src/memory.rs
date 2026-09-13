use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};

pub struct MemorySensor;

impl MemorySensor {
    pub fn new() -> Self { Self }
}

impl SensorProvider for MemorySensor {
    fn name(&self) -> &str { "memory" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        #[cfg(target_os = "windows")]
        {
            use windows::Win32::System::SystemInformation::{
                GlobalMemoryStatusEx, MEMORYSTATUSEX,
            };

            unsafe {
                let mut mem = MEMORYSTATUSEX::default();
                mem.dwLength = std::mem::size_of::<MEMORYSTATUSEX>() as u32;

                if GlobalMemoryStatusEx(&mut mem).is_ok() {
                    readings.push(SensorReading {
                        sensor: "ramTotalBytes".into(),
                        value: mem.ullTotalPhys as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "ramUsedBytes".into(),
                        value: (mem.ullTotalPhys - mem.ullAvailPhys) as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "ramAvailBytes".into(),
                        value: mem.ullAvailPhys as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "memoryLoadPct".into(),
                        value: mem.dwMemoryLoad as f64,
                        unit: "%".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "commitTotalBytes".into(),
                        value: mem.ullTotalPageFile as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "commitUsedBytes".into(),
                        value: (mem.ullTotalPageFile - mem.ullAvailPageFile) as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });
                }
            }
        }

        SensorBatch {
            readings,
            collected_at: ts,
            collection_duration_us: start.elapsed().as_micros() as u64,
        }
    }

    fn is_available(&self) -> bool { true }
}

impl Default for MemorySensor {
    fn default() -> Self { Self::new() }
}