use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};

pub struct ThermalSensor;

impl ThermalSensor {
    pub fn new() -> Self { Self }
}

impl SensorProvider for ThermalSensor {
    fn name(&self) -> &str { "thermal" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        #[cfg(target_os = "windows")]
        {
            use windows::Win32::System::SystemInformation::{
                GetPhysicallyInstalledSystemMemory,
            };

            unsafe {
                let mut ram_kb = 0u64;
                if GetPhysicallyInstalledSystemMemory(&mut ram_kb).is_ok() {
                    readings.push(SensorReading {
                        sensor: "ramInstalledKB".into(),
                        value: ram_kb as f64,
                        unit: "KB".into(),
                        timestamp: ts,
                        valid: true,
                    });
                }
            }

            if let Ok(temp) = std::fs::read_to_string("/sys/class/thermal/thermal_zone0/temp") {
                if let Ok(t) = temp.trim().parse::<f64>() {
                    readings.push(SensorReading {
                        sensor: "cpuCoreTempC".into(),
                        value: t / 1000.0,
                        unit: "C".into(),
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

impl Default for ThermalSensor {
    fn default() -> Self { Self::new() }
}