# Parcelon

The _Parcelon_ convention brings object-based programming to C using the [_Parcel_](https://github.com/nbyoung/parcel) encapsulation language. It formalises the separation of an abstract _interface_ from its concrete _state_ and _method_ implementations, enabling polymorphic source components expressible entirely in standard C without a run-time library.

## Overview

An **object type** comprises two elements held in separate source files:

- An **object file** declares the _method table_: a `typedef` for a struct of function pointers, each taking a type-erased `void *self` state pointer as its first argument.
- An **implement file** imports the method table typedef, defines a concrete _state_ struct, implements each method as a static function with a typed self pointer, binds them into a `const`-qualified method table instance, and exports the state typedef and instance together.

A caller imports an implement parcel and dispatches methods by dereferencing the method table constant through the import stem.

## Object Files

An object file exports a single typedef naming the method table struct. The parcel name is conventionally `_`, indicating the one interface defined at this path.

```c
typedef struct {
    int (*init)    (void *self);
    <R> (*<method>)(void *self, <params>);
    int (*fini)    (void *self);
} <Name>;

#pragma  parcel _
#pragma      typedef: <Name>
#include "export/<path>/_"
```

`init` and `fini` initialise and finalise the state; additional methods perform the type's operations. All methods return `int`: non-zero for success, zero for failure.

## Implement Files

An implement file imports the object parcel with a chosen stem, defines the state struct and static method bodies, binds them to a `const` method table instance, and exports the state typedef and instance:

```c
#include "import/<path>/_.<stem>"

typedef struct { <fields> } <Name>;

static int    init    (<Name> *self) { ... }
static <R>    <impl_method>(<Name> *self, <params>) { ... }
static int    fini    (<Name> *self) { ... }

const <stem>_<InterfaceName> <name> = {
    .init     = (int    (*)(void *))           init,
    .<method> = (<R>    (*)(void *, <params>)) <impl_method>,
    .fini     = (int    (*)(void *))           fini,
};

#pragma  parcel <impl>
#pragma      typedef: <Name>
#pragma      constant: <name>
#include "export/<path>/<impl>"
```

`<Name>` is the concrete state type; `<name>` is the method table instance, named by lowercasing the interface identifier (`Output` → `output`). The parcel name `<impl>` is the implementation's distinguishing identifier, used in the export path and canonical name.

When a static method implementation shares its name with the method table instance — as occurs when the primary method has the same name as the interface — the implementation function must be given a distinct identifier (shown here as `<impl_method>`) to avoid a file-scope naming conflict in C.

Each method is defined with a typed `<Name> *self` parameter and cast to the erased `void *` signature at the initialiser. The cast is necessary because C does not implicitly convert `<Name> *` to `void *` in a function-pointer context.

## Caller Pattern

A caller imports an implement parcel with a freely chosen stem. The stem scopes the state typedef and the stem pointer through which methods are dispatched:

```c
#include "import/<path>/<impl>.<stem>"

<stem>_<Name> state;

<stem>-><name>->init    (&state);
<stem>-><name>-><method>(&state, <args>);
<stem>-><name>->fini    (&state);
```

`<stem>` is the `const` pointer to the canonical struct defined in the import file. Because `<name>` is a `const`-qualified exported identifier, the Parcel import struct holds it as a `const`-pointer member; it is therefore dereferenced with `->` to reach the method table, and `->` again to call through a function pointer.

## Polymorphism

A _selector header_ allows a caller to switch between implementations at compile time without changing source code. It conditionally includes one implementation import file and defines two macros:

- `OUT` — the stem pointer to the selected implementation's canonical struct
- `OUT_STATE` — the state typedef for the selected implementation

```c
#include "import/<path>/_.out"

#if defined(OUTPUT_NULL)
#include "import/<path>/null.null"
#define OUT       null
#define OUT_STATE null_Null
#else
#include "import/<path>/stdout.std"
#define OUT       std
#define OUT_STATE std_Stdout
#endif
```

A caller that includes the selector header is independent of any specific implementation:

```c
#include "output.h"

OUT_STATE state;
OUT->output->init(&state);
OUT->output->output(&state, "Hello, world!\n");
OUT->output->fini(&state);
```

The implementation is selected by passing a preprocessor definition on the compiler command line:

```
cc -DOUTPUT_NULL ...    # selects the null implementation
cc ...                  # selects the default (stdout) implementation
```

## Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Interface typedef | `<Name>` | `Output` |
| Method table instance | `<name>` (lowercase of interface identifier) | `output` |
| State typedef | `<Name>` | `Stdout`, `Null` |
| Object parcel name | `_` | — |
| Interface import stem | `<stem>` (chosen by implementer) | `out_Stdout` |
| Implementation parcel name | implementation identifier | `stdout`, `null` |

## Parcel Translation

### Interface typedef

The object file's parcel exports only a typedef. Per _Parcel_ semantics, typedefs have no representation in the export file; the translator generates only the import file:

```
_parcel/export/<path>/_        (empty)
_parcel/import/<path>/_.<stem> → typedef struct { ... } <stem>_<Name>;
```

### Method table constant

The implement file's parcel exports two identifiers: a typedef (`<Name>`) and a constant (`<name>`). The translator generates:

```
_parcel/export/<path>/<impl>:
    struct <canonical> {
        const <stem>_<Name> * const <name>;
    } const <canonical> = { &<name> };

_parcel/import/<path>/<impl>.<stem>:
    typedef struct { } <stem>_<Name>;
    struct <canonical> {
        const struct { <expanded method table> } * const <name>;
    };
    extern const struct <canonical> <canonical>;
    const struct <canonical> *<stem> = &<canonical>;
```

The import file applies _type specifier expansion_ (see [SEMANTICS.md](https://github.com/nbyoung/parcel/blob/main/SEMANTICS.md)) to replace `<stem>_<InterfaceName>` with the expanded anonymous struct, making the import file self-contained with no dependency on the object parcel's import.

## Example

`examples/hello_world/` demonstrates the `Output` object type with `stdout` and `null` implementations.

`output.c` declares the `Output` interface — `init`, `output`, and `fini` — as the default parcel (`_`):

```c
typedef struct {
    int (*init)  (void *self);
    int (*output)(void *self, char *greeting);
    int (*fini)  (void *self);
} Output;

#pragma  parcel _
#pragma      typedef: Output
#include "export/output/_"
```

`output/stdout.c` implements `Output` as `Stdout`, backed by C standard I/O. It imports the interface with stem `out`, declares the concrete state struct, and exports the state typedef and method table instance together:

```c
#include <stdio.h>
#include "import/output/_.out"

typedef struct {
} Stdout;

static int init(Stdout *self) {
    (void)self;
    return 1;
}

static int print(Stdout *self, char *greeting) {
    (void)self;
    return fputs(greeting, stdout) != EOF;
}

static int fini(Stdout *self) {
    (void)self;
    return fflush(stdout) != EOF;
}

const out_Output output = {
    .init   = (int (*)(void *))         init,
    .output = (int (*)(void *, char *)) print,
    .fini   = (int (*)(void *))         fini,
};

#pragma  parcel stdout
#pragma      typedef: Stdout
#pragma      constant: output
#include "export/output/stdout"
```

`output/null.c` exports an identically shaped parcel with bodies that do nothing — a valid substitute that requires no changes at the call sites:

```c
#include "import/output/_.out"

typedef struct {
} Null;

static int init(Null *self) {
    (void)self;
    return 1;
}

static int discard(Null *self, char *greeting) {
    (void)self;
    (void)greeting;
    return 1;
}

static int fini(Null *self) {
    (void)self;
    return 1;
}

const out_Output output = {
    .init   = (int (*)(void *))         init,
    .output = (int (*)(void *, char *)) discard,
    .fini   = (int (*)(void *))         fini,
};

#pragma  parcel null
#pragma      typedef: Null
#pragma      constant: output
#include "export/output/null"
```

`output.h` is a selector header that conditionally imports one implementation and defines `OUT` and `OUT_STATE`:

```c
#include "import/output/_.out"

#if defined(OUTPUT_NULL)
#include "import/output/null.null"
#define OUT       null
#define OUT_STATE null_Null
#else
#include "import/output/stdout.std"
#define OUT       std
#define OUT_STATE std_Stdout
#endif
```

`main.c` includes the selector and dispatches through `OUT`, naming neither the implementation nor its stem directly:

```c
#include "output.h"

int main(void) {
    OUT_STATE state;
    OUT->output->init(&state);
    OUT->output->output(&state, "Hello, world!\n");
    OUT->output->fini(&state);
    return 0;
}
```

Passing `-DOUTPUT_NULL` selects the `null` implementation; omitting it selects `stdout`. `main.c` is unchanged in either case.
