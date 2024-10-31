# How-To Guide

## Context

* Include the concurrency support header file
```cpp
#include "concurrency.h"
```
* The main app context has to be created as shown below
```cpp
auto ctx = context::WithCancel(context::Background()); // Correct example.
```
* Never use `context::Background()` directly. The following example is wrong
```cpp
auto ctx = context::Background(); // Bad example, don't do that.
```

* Pass `context::Context& ctx` as an argument to function calls if you want them to support graceful centralized cancellation. Then, check `ctx.cancelled()` periodically and exit if the result is `true`. Pass `ctx` to other function calls to propagate the cancellation event.
* When you want to cancel some processing automatically after some time, create a child context and pass it to the function call the following way
```cpp
// Assume ctx is the main context created somewhere at the app startup.
auto tctx = context::WithTimeout(ctx, std::chrono::milliseconds(1000));
process_user_data(tctx, user_data);
...
```
```cpp
void process_user_data(context::Context ctx, void *data)
{
    process_stage_1(data);
    if (ctx.cancelled())
        return;

    process_stage_2(data);
    if (ctx.cancelled())
        return

    process_stage_3(data);
    if (ctx.cancelled())
        return;

    process_stage_4(data);
    if (ctx.cancelled())
        return;

    process_stage_5(data);
}
```
* You can cancel any context explicitly by calling `ctx.cancel()`.
* If the top level (main) context is cancelled, all child contexts are cancelled recursively as well. This is called **Graceful Shutdown**.
* Always pass the context by reference. Don't use pointers to the context.


