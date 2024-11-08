// this worked on Nov 7 2024
// tested on binji.github.io/wasm-clang

// task manager takes a list of repeat periods and callback functions
// these should run in the background
// set an alarm to run the "on_irq" boolean every "tick" microseconds.
// load in the background functions you want using the "bind" language.

#pragma once
#include <vector>
#include <functional>
#include "syntacticSugar.h"
#include "hardware/timer.h"
#include "hardware/irq.h"       // library of code to let you interrupt code execution to run something of higher priority

const int frequency_poll = 32;

static void on_irq();

class task_mgr_obj {
    private:
        struct task_obj {
            int counter = 0;
            int period;
            std::function<void()> exec_on_trigger;
            void set_period(int arg_period) {
                period = arg_period;
            }
            void set_trigger(std::function<void()> arg_func) {
                exec_on_trigger = arg_func;
            }
            void increment(int add_uS) {
                counter += add_uS;
            }
            void execute() {
                exec_on_trigger();
            }
            bool triggered() {
                if (counter >= period) {
                    counter = counter % period;
                    return true;
                }
                return false;
            }
        };
        int tick_uS = 0;
        byte alarm_ID = 0;
    public:
        task_mgr_obj(int arg_uS) {
            tick_uS = arg_uS;
        }
        std::vector<task_obj> task_list;
        int get_tick_uS() {
            return tick_uS;
        }
        void add_task(int arg_repeat_uS, std::function<void()> arg_on_trigger) {
            task_obj new_task;
            new_task.set_period(arg_repeat_uS);
            new_task.set_trigger(arg_on_trigger);
            task_list.emplace_back(new_task);
        }
        void set_timer() {
          timer_hw->alarm[alarm_ID] = getTheCurrentTime() + tick_uS;
        }
        void begin() {
          hw_set_bits(&timer_hw->inte, 1u << alarm_ID);  // initialize the timer
          irq_set_exclusive_handler(alarm_ID, on_irq);     // function to run every interrupt
          irq_set_enabled(alarm_ID, true);               // ENGAGE!
          set_timer();
        }
        void repeat_timer() {
          hw_clear_bits(&timer_hw->intr, 1u << alarm_ID);
          set_timer();
        }
};

task_mgr_obj task_mgr(frequency_poll);

static void on_irq() {
    task_mgr.repeat_timer();
    int t = task_mgr.get_tick_uS();
    for (auto &i : task_mgr.task_list) {
        i.increment(t);
    }
    for (auto &i : task_mgr.task_list) {
        if (i.triggered()) {
            i.execute();
            break;
        }
    }
}
