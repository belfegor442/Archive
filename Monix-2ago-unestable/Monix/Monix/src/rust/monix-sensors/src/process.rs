use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub cpu_pct: f64,
    pub ram_bytes: u64,
    pub thread_count: u32,
}

pub struct ProcessSensor {
    prev_samples: std::collections::HashMap<u32, ProcessCpuSample>,
}

struct ProcessCpuSample {
    cpu_time_100ns: u64,
    timestamp: u64,
}

impl ProcessSensor {
    pub fn new() -> Self {
        Self {
            prev_samples: std::collections::HashMap::new(),
        }
    }
}

impl SensorProvider for ProcessSensor {
    fn name(&self) -> &str { "process" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        #[cfg(target_os = "windows")]
        {
            use windows::Win32::System::Threading::{
                CreateToolhelp32Snapshot, Process32FirstW, Process32NextW,
                TH32CS_SNAPPROCESS, PROCESSENTRY32W,
            };
            use windows::Win32::Foundation::{CloseHandle, HANDLE};
            use windows::Win32::System::Threading::{OpenProcess, PROCESS_QUERY_INFORMATION, PROCESS_VM_READ};
            use windows::Win32::System::Threading::{GetProcessTimes, FILETIME};

            unsafe {
                let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if snapshot == HANDLE(0) {
                    return SensorBatch {
                        readings,
                        collected_at: ts,
                        collection_duration_us: start.elapsed().as_micros() as u64,
                    };
                }

                let mut entry = PROCESSENTRY32W::default();
                entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;

                let mut process_count = 0u32;
                let mut total_threads = 0u32;

                if Process32FirstW(snapshot, &mut entry).is_ok() {
                    loop {
                        process_count += 1;
                        total_threads += entry.th32ThreadCount;

                        if Process32NextW(snapshot, &mut entry).is_err() {
                            break;
                        }
                    }
                }

                CloseHandle(snapshot).ok();

                readings.push(SensorReading {
                    sensor: "processCount".into(),
                    value: process_count as f64,
                    unit: "count".into(),
                    timestamp: ts,
                    valid: true,
                });

                readings.push(SensorReading {
                    sensor: "threadCount".into(),
                    value: total_threads as f64,
                    unit: "count".into(),
                    timestamp: ts,
                    valid: true,
                });
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

impl Default for ProcessSensor {
    fn default() -> Self { Self::new() }
}