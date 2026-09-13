pub mod timeline;
pub mod entity;
pub mod change;

use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::Arc;

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct TelemetryValue {
    pub field: String,
    pub value: f64,
    pub timestamp: u64,
    pub valid: bool,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct Snapshot {
    pub id: u64,
    pub timestamp: u64,
    pub values: HashMap<String, TelemetryValue>,
}

impl Snapshot {
    pub fn new(id: u64, timestamp: u64) -> Self {
        Self {
            id,
            timestamp,
            values: HashMap::new(),
        }
    }

    pub fn set(&mut self, field: &str, value: f64, valid: bool) {
        self.values.insert(field.to_string(), TelemetryValue {
            field: field.to_string(),
            value,
            timestamp: self.timestamp,
            valid,
        });
    }

    pub fn get(&self, field: &str) -> Option<f64> {
        self.values.get(field).filter(|v| v.valid).map(|v| v.value)
    }

    pub fn get_with_validity(&self, field: &str) -> Option<&TelemetryValue> {
        self.values.get(field)
    }
}

pub struct StateStore {
    timeline: RwLock<timeline::TimelineRing>,
    entities: RwLock<entity::EntityTracker>,
    last_changes: RwLock<change::ChangeSet>,
}

impl StateStore {
    pub fn new(capacity: usize) -> Self {
        Self {
            timeline: RwLock::new(timeline::TimelineRing::new(capacity)),
            entities: RwLock::new(entity::EntityTracker::new()),
            last_changes: RwLock::new(change::ChangeSet::new()),
        }
    }

    pub fn update(&self, snapshot: Snapshot) {
        let prev = {
            let timeline = self.timeline.read();
            timeline.current().cloned()
        };

        let changes = {
            let mut entities = self.entities.write();
            entities.detect_changes(&snapshot, prev.as_ref())
        };

        {
            let mut last_changes = self.last_changes.write();
            *last_changes = changes;
        }

        {
            let mut timeline = self.timeline.write();
            timeline.push(snapshot);
        }
    }

    pub fn current(&self) -> Option<Snapshot> {
        self.timeline.read().current().cloned()
    }

    pub fn previous(&self) -> Option<Snapshot> {
        self.timeline.read().previous().cloned()
    }

    pub fn history_size(&self) -> usize {
        self.timeline.read().count()
    }

    pub fn last_changes(&self) -> change::ChangeSet {
        self.last_changes.read().clone()
    }
}

impl Default for StateStore {
    fn default() -> Self {
        Self::new(64)
    }
}