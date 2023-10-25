#ifndef PLUG_H_
#define PLUG_H_

#define LIST_OF_PLUGS \
    PLUG(sch_init, void, void) \
    PLUG(sch_pre_reload, void*, void) \
    PLUG(sch_post_reload, void, void*) \
    PLUG(sch_update, void, void)

#define PLUG(name, ret, ...) typedef ret (name##_t)(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#endif // PLUG_H_
