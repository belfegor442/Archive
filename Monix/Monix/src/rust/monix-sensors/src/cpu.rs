use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};
use std::collections::HashMap;

pub struct CpuSensor {
    prev_idle: u64,
    prev_total: u64,
}

impl CpuSensor {
    pub fn new() -> Self {
        Self {
            prev_idle: 0,
            prev_total: 0,
        }
    }

    fn read_cpu_times() -> Option<(u64, u64)> {
        #[cfg(target_os = "windows")]
        {
            use std::mem;
            use windows::Win32::System::SystemInformation::{
                GetSystemTimes, FILETIME,
            };

            unsafe {
                let mut idle_time = FILETIME::default();
                let mut kernel_time = FILETIME::default();
                let mut user_time = FILETIME::default();

                if GetSystemTimes(&mut idle_time, &mut kernel_time, &mut user_time).is_ok() {
                    let idle = ((idle_time.dwHighDateTime as u64) << 32) | (idle_time.dwLowDateTime as u64);
                    let kernel = ((kernel_time.dwHighDateTime as u64) << 32) | (kernel_time.dwLowDateTime as u64);
                    let user = ((user_time.dwHighDateTime as u64) << 32) | (user_time.dwLowDateTime as u64);
                    let total = kernel + user;
                    return Some((idle, total));
                }
            }
            None
        }
        #[cfg(not(target_os = "windows"))]
        None
    }
}

impl SensorProvider for CpuSensor {
    fn name(&self) -> &str { "cpu" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        if let Some((idle, total)) = Self::read_cpu_times() {
            let idle_delta = idle.saturating_sub(self.prev_idle);
            let total_delta = total.saturating_sub(self.prev_total);

            if total_delta > 0 {
                let usage = ((total_delta - idle_delta) as f64 / total_delta as f64) * 100.0;
                readings.push(SensorReading {
                    sensor: "cpuPct".into(),
                    value: usage.clamp(0.0, 100.0),
                    unit: "%".into(),
                    timestamp: ts,
                    valid: true,
                });
            }
        }

        readings.push(SensorReading {
            sensor: "cpuCores".into(),
            value: num_cpus::get() as f64,
            unit: "count".into(),
            timestamp: ts,
            valid: true,
        });

        SensorBatch {
            readings,
            collected_at: ts,
            collection_duration_us: start.elapsed().as_micros() as u64,
        }
    }

    fn is_available(&self) -> bool { true }
}

impl Default for CpuSensor {
    fn default() -> Self { Self::new() }
}