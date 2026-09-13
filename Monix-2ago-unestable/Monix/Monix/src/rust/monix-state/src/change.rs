use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ChangeKind {
    Created,
    Terminated,
    Modified,
    StateChanged,
    ValueChanged,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChangeEvent {
    pub kind: ChangeKind,
    pub entity_type: String,
    pub entity_id: String,
    pub attribute: String,
    pub timestamp: u64,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ChangeSet {
    pub events: Vec<ChangeEvent>,
}

impl ChangeSet {
    pub fn new() -> Self {
        Self { events: Vec::new() }
    }

    pub fn is_empty(&self) -> bool {
        self.events.is_empty()
    }

    pub fn count(&self) -> usize {
        self.events.len()
    }

    pub fn clear(&mut self) {
        self.events.clear();
    }

    pub fn add_created(&mut self, entity_type: &str, entity_id: &str, attribute: &str, ts: u64) {
        self.events.push(ChangeEvent {
            kind: ChangeKind::Created,
            entity_type: entity_type.into(),
            entity_id: entity_id.into(),
            attribute: attribute.into(),
            timestamp: ts,
        });
    }

    pub fn add_terminated(&mut self, entity_type: &str, entity_id: &str, attribute: &str, ts: u64) {
        self.events.push(ChangeEvent {
            kind: ChangeKind::Terminated,
            entity_type: entity_type.into(),
            entity_id: entity_id.into(),
            attribute: attribute.into(),
            timestamp: ts,
        });
    }

    pub fn add_modified(&mut self, entity_type: &str, entity_id: &str, attribute: &str, ts: u64) {
        self.events.push(ChangeEvent {
            kind: ChangeKind::Modified,
            entity_type: entity_type.into(),
            entity_id: entity_id.into(),
            attribute: attribute.into(),
            timestamp: ts,
        });
    }

    pub fn add_value_changed(&mut self, entity_type: &str, entity_id: &str, attribute: &str, ts: u64) {
        self.events.push(ChangeEvent {
            kind: ChangeKind::ValueChanged,
            entity_type: entity_type.into(),
            entity_id: entity_id.into(),
            attribute: attribute.into(),
            timestamp: ts,
        });
    }

    pub fn merge(&mut self, other: ChangeSet) {
        self.events.extend(other.events);
    }
}