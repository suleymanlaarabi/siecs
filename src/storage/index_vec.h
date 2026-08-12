#ifndef SIECS_STORAGE_INDEX_VEC_H
#define SIECS_STORAGE_INDEX_VEC_H

#define ECS_INDEX_VEC_ID_VALID(fn, id_type, vec_expr) \
    static inline bool fn(id_type id) { \
        return id != 0 && id < (vec_expr).size; \
    }

#define ECS_INDEX_VEC_GET_MUT(fn, id_type, value_type, vec_expr) \
    static inline value_type *fn(id_type id) { \
        return sicore_vec_get_mut(&(vec_expr), id, value_type); \
    }

#define ECS_INDEX_VEC_GET(fn, id_type, value_type, vec_expr) \
    static inline const value_type *fn(id_type id) { \
        return sicore_vec_get(&(vec_expr), id, value_type); \
    }

#endif
