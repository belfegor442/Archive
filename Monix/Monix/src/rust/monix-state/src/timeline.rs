use crate::Snapshot;

pub struct TimelineRing {
    ring: Vec<Option<Snapshot>>,
    capacity: usize,
    head: usize,
    count: usize,
}

impl TimelineRing {
    pub fn new(capacity: usize) -> Self {
        let mut ring = Vec::with_capacity(capacity);
        ring.resize_with(capacity, || None);
        Self {
            ring,
            capacity,
            head: 0,
            count: 0,
        }
    }

    pub fn push(&mut self, snapshot: Snapshot) {
        self.ring[self.head] = Some(snapshot);
        self.head = (self.head + 1) % self.capacity;
        if self.count < self.capacity {
            self.count += 1;
        }
    }

    pub fn current(&self) -> Option<&Snapshot> {
        if self.count == 0 {
            return None;
        }
        let idx = (self.head + self.capacity - 1) % self.capacity;
        self.ring[idx].as_ref()
    }

    pub fn previous(&self) -> Option<&Snapshot> {
        if self.count < 2 {
            return None;
        }
        let idx = (self.head + self.capacity - 2) % self.capacity;
        self.ring[idx].as_ref()
    }

    pub fn at(&self, offset: usize) -> Option<&Snapshot> {
        if offset >= self.count {
            return None;
        }
        let idx = (self.head + self.capacity - 1 - offset) % self.capacity;
        self.ring[idx].as_ref()
    }

    pub fn count(&self) -> usize { self.count }
    pub fn capacity(&self) -> usize { self.capacity }
    pub fn is_full(&self) -> bool { self.count == self.capacity }

    pub fn clear(&mut self) {
        self.head = 0;
        self.count = 0;
        for slot in &mut self.ring {
            *slot = None;
        }
    }
}

impl Default for TimelineRing {
    fn default() -> Self {
        Self::new(64)
    }
}