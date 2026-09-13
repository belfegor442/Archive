use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};

pub struct DiskSensor;

impl DiskSensor {
    pub fn new() -> Self { Self }
}

impl SensorProvider for DiskSensor {
    fn name(&self) -> &str { "disk" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        #[cfg(target_os = "windows")]
        {
            use windows::Win32::Storage::FileSystem::{
                GetDiskFreeSpaceExW, GetLogicalDriveStringsW,
            };

            unsafe {
                let mut free_bytes = 0u64;
                let mut total_bytes = 0u64;
                let mut total_free = 0u64;

                if GetDiskFreeSpaceExW(
                    "C:\\",
                    Some(&mut free_bytes),
                    Some(&mut total_bytes),
                    Some(&mut total_free),
                ).is_ok() {
                    readings.push(SensorReading {
                        sensor: "diskTotalBytes".into(),
                        value: total_bytes as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "diskFreeBytes".into(),
                        value: free_bytes as f64,
                        unit: "bytes".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    let used_pct = if total_bytes > 0 {
                        ((total_bytes - free_bytes) as f64 / total_bytes as f64) * 100.0
                    } else {
                        0.0
                    };

                    readings.push(SensorReading {
                        sensor: "diskPctUsed".into(),
                        value: used_pct,
                        unit: "%".into(),
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

impl Default for DiskSensor {
    fn default() -> Self { Self::new() }
}