use crate::{SensorBatch, SensorProvider, SensorReading, monotonic_ns};
use std::collections::HashMap;

pub struct NetworkSensor {
    prev_bytes_in: u64,
    prev_bytes_out: u64,
    prev_connections: u32,
}

impl NetworkSensor {
    pub fn new() -> Self {
        Self {
            prev_bytes_in: 0,
            prev_bytes_out: 0,
            prev_connections: 0,
        }
    }
}

impl SensorProvider for NetworkSensor {
    fn name(&self) -> &str { "network" }

    fn collect(&self) -> SensorBatch {
        let start = std::time::Instant::now();
        let ts = monotonic_ns();
        let mut readings = Vec::new();

        #[cfg(target_os = "windows")]
        {
            use windows::Win32::NetworkManagement::IpHelper::{
                GetAdaptersInfo, GetTcpStatistics, IP_ADAPTER_INFO, MIB_TCPSTATS,
            };

            unsafe {
                let mut tcp_stats = MIB_TCPSTATS::default();
                if GetTcpStatistics(&mut tcp_stats).is_ok() {
                    readings.push(SensorReading {
                        sensor: "tcpEstablished".into(),
                        value: tcp_stats.dwNumConns as f64,
                        unit: "count".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "tcpResets".into(),
                        value: tcp_stats.dwOutRsts as f64,
                        unit: "count".into(),
                        timestamp: ts,
                        valid: true,
                    });

                    readings.push(SensorReading {
                        sensor: "tcpRetransmits".into(),
                        value: tcp_stats.dwRetransSegs as f64,
                        unit: "count".into(),
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

impl Default for NetworkSensor {
    fn default() -> Self { Self::new() }
}