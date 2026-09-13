use crate::{Snapshot, change::{ChangeSet, ChangeEvent, ChangeKind}};
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone)]
pub struct TrackedProcess {
    pub pid: u32,
    pub name: String,
    pub first_seen: u64,
    pub last_seen: u64,
    pub cpu_delta: f64,
    pub ram_delta: u64,
}

#[derive(Debug, Clone)]
pub struct TrackedDriver {
    pub name: String,
    pub first_seen: u64,
    pub last_seen: u64,
}

pub struct EntityTracker {
    processes: HashMap<u32, TrackedProcess>,
    drivers: HashMap<String, TrackedDriver>,
}

impl EntityTracker {
    pub fn new() -> Self {
        Self {
            processes: HashMap::new(),
            drivers: HashMap::new(),
        }
    }

    pub fn detect_changes(&mut self, current: &Snapshot, previous: Option<&Snapshot>) -> ChangeSet {
        let mut changes = ChangeSet::new();
        let ts = current.timestamp;

        let current_pid_set: HashSet<u32> = current.values.iter()
            .filter_map(|(k, _)| {
                if k.starts_with("proc_") {
                    k.strip_prefix("proc_")?.split('_').next()?.parse().ok()
                } else {
                    None
                }
            })
            .collect();

        for &pid in &current_pid_set {
            if !self.processes.contains_key(&pid) {
                let name = current.values.get(&format!("proc_{}_name", pid))
                    .map(|v| format!("{}", v.value))
                    .unwrap_or_default();

                self.processes.insert(pid, TrackedProcess {
                    pid,
                    name,
                    first_seen: ts,
                    last_seen: ts,
                    cpu_delta: 0.0,
                    ram_delta: 0,
                });

                changes.events.push(ChangeEvent {
                    kind: ChangeKind::Created,
                    entity_type: "process".into(),
                    entity_id: pid.to_string(),
                    attribute: "pid".into(),
                    timestamp: ts,
                });
            } else if let Some(proc) = self.processes.get_mut(&pid) {
                proc.last_seen = ts;
            }
        }

        let terminated: Vec<u32> = self.processes.keys()
            .filter(|pid| !current_pid_set.contains(pid))
            .cloned()
            .collect();

        for pid in terminated {
            if let Some(proc) = self.processes.remove(&pid) {
                changes.events.push(ChangeEvent {
                    kind: ChangeKind::Terminated,
                    entity_type: "process".into(),
                    entity_id: pid.to_string(),
                    attribute: "pid".into(),
                    timestamp: ts,
                });
            }
        }

        changes
    }

    pub fn process_count(&self) -> usize { self.processes.len() }
    pub fn driver_count(&self) -> usize { self.drivers.len() }

    pub fn processes(&self) -> &HashMap<u32, TrackedProcess> { &self.processes }
    pub fn drivers(&self) -> &HashMap<String, TrackedDriver> { &self.drivers }
}

impl Default for EntityTracker {
    fn default() -> Self { Self::new() }
}