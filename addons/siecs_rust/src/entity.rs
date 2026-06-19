use crate::raw;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct Entity {
    pub(crate) id: raw::EntityId,
}

impl Entity {
    #[inline]
    pub(crate) const fn from_raw(id: raw::EntityId) -> Self {
        Self { id }
    }

    #[inline]
    pub const fn id(self) -> raw::EntityId {
        self.id
    }
}
