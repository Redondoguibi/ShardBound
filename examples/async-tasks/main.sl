@Globals
job = null
completed = 0

async func delayed_steps(count = 3):
    for index in number_range(count):
        await sleep(0.25)
        completed = completed + 1
        log("async_demo: completed another step")
    return completed

@Events
on_ready():
    job = delayed_steps()

on_shutdown():
    if job != null:
        task_cancel(job)
