#include "timer.h"
#include "timer_store.h"
#include "timer_handler.h"
#include "replicator.h"
#include "callback.h"
#include "http_callback.h"
#include "controller.h"

#include <iostream>
#include <cassert>

#include "time.h"

Timer* default_timer(TimerID id)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return new Timer(id,
                   (ts.tv_sec * 1000) + (ts.tv_nsec / 1000),
                   100,
                   100,
                   0,
                   std::vector<std::string>(1, "10.0.0.1"),
                   "localhost:80/callback",
                   "stuff stuff stuff");
}

int main(int argc, char** argv)
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
  {
    Timer* timer = default_timer(1);
    timer->start_time = 123456789;
    assert(timer->url("localhost") == "http://localhost/timers/1");
    assert(timer->to_json() == "{\"timing\":{\"start-at\":\"123456789\",\"sequence-number\":\"0\",\"interval\":\"100\",\"repeat-for\":\"100\"},\"callback\":{\"http\":{\"uri\":\"localhost:80/callback\",\"opaque\":\"stuff stuff stuff\"}},\"reliability\":{\"replicas\":[\"10.0.0.1\"]}}");
    delete timer;
  }

  TimerStore* store = new TimerStore();

  // Test 1 - Adding a timer and then retrieving the next set returns
  //          the added timer.
  {
    Timer* timer = default_timer(1);
    store->add_timer(timer);
    std::unordered_set<Timer*> next_timers;
    store->get_next_timers(next_timers);
    assert(next_timers.size() == 1);
    timer = *next_timers.begin();
    assert(timer->id = 1);
    delete timer;
  }

  // Test 2 - Adding two timers with the same timestamp causes them to appear in the
  //          same returned set.
  {
    Timer* timer = default_timer(1);
    Timer* timer2 = default_timer(2);
    timer2->start_time = timer->start_time;
    store->add_timer(timer);
    store->add_timer(timer2);
    std::unordered_set<Timer*> next_timers;
    store->get_next_timers(next_timers);

    assert(next_timers.size() == 2);
    
    for (auto it = next_timers.begin(); it != next_timers.end(); it++)
    {
      delete (*it);
    }
    next_timers.clear();
  }

  // Test 3 - Leak test.  Add timer then destroy the store.
  {
    TimerStore* store = new TimerStore();
    Timer* timer = default_timer(1);
    store->add_timer(timer);
    delete store;
  }

  // Test 4 - Leak test.  Add timer to the handler then destroy the handler.
  {
    HTTPCallback* callback = new HTTPCallback();
    Replicator* replicator = new Replicator();
    TimerHandler* handler = new TimerHandler(store, replicator, callback);
    Timer* timer = default_timer(1);
    store->add_timer(timer);
    delete handler;
  }

  delete store;

  Controller* controller = new Controller();
  struct event_base* base;
  struct evhttp* http;

  // Create an event reactor.
  base = event_base_new();
  if (!base) {
    fprintf(stderr, "Couldn't create an event_base: exiting\n");
    return 1;
  }

  // Create an HTTP server instance.
  http = evhttp_new(base);
  if (!http) {
    fprintf(stderr, "couldn't create evhttp. Exiting.\n");
    return 1;
  }

  // Register a callback for the "/ping" path.
  evhttp_set_cb(http, "/ping", Controller::controller_ping_cb, NULL);

  // Register a callback for the "/timers" path, we have to do this with the
  // generic callback as libevent doesn't support regex paths.
  evhttp_set_gencb(http, Controller::controller_cb, controller);

  // Bind to the correct port
  evhttp_bind_socket(http, "0.0.0.0", 7253);

  // Start the reactor, this blocks the current thread
  event_base_dispatch(base);

  curl_global_cleanup();

  return 0;
}
