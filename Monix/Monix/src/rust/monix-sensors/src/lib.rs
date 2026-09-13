pub mod cpu;
pub mod memory;
pub mod disk;
pub mod network;
pub mod thermal;
pub mod process;

use serde::{Deserialize, Serialize};
use std::time::{Duration, Instant};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorReading {
    pub sensor: String,
    pub value: f64,
    pub unit: String,
    pub timestamp: u64,
    pub valid: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorBatch {
    pub readings: Vec<SensorReading>,
    pub collected_at: u64,
    pub collection_duration_us: u64,
}

pub trait SensorProvider: Send + Sync {
    fn name(&self) -> &str;
    fn collect(&self) -> SensorBatch;
    fn is_available(&self) -> bool;
}

pub fn monotonic_ns() -> u64 {
    static START: std::sync::OnceLock<Instant> = std::sync::OnceLock::new();
    let start = START.get_or_init(Instant::now);
    start.elapsed().as_nanos() as u64
}

pub fn wall_ms() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}