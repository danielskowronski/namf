//
// Created by viciu on 08.06.2020.
//
#include "scheduler.h"
#include "helpers.h"
#include "components.h"
#include "lang/select_lang.h"


namespace SimpleScheduler {
    unsigned long nullF(LoopEventType event) { return 0; }


    NAMFScheduler::NAMFScheduler() {
        loopSize = 0;
        sensorsWithDisplays = 0;
        for (byte i = 0; i < SCHEDULER_SIZE; i++) {
            _tasks[i].process = nullF;
            _tasks[i].nextRun = 0;
            _tasks[i].slotID = EMPTY;

        }
    }

    void NAMFScheduler::process() {
        for (byte i = 0; i < SCHEDULER_SIZE; i++) {
            if (_tasks[i].nextRun && _tasks[i].nextRun < millis()) {
                unsigned long nextRun = _tasks[i].process(RUN);
                if (nextRun) {
                    _tasks[i].nextRun = millis() + nextRun;
                } else {
                    _tasks[i].nextRun = 0;
                }
            }

        }

    }

    void NAMFScheduler::init(LoopEntryType slot) {
        if (slot == EMPTY) return;
        int i = findSlot(slot);
        if (i < 0) return;
        unsigned long nextRun = _tasks[i].process(INIT);
        if (nextRun) {
            _tasks[i].nextRun = millis() + nextRun;
        } else {
            _tasks[i].nextRun = 0;
        }
        _tasks[i].hasDisplay = false;


    }

    void NAMFScheduler::init() {
        for (byte i = 0; i < SCHEDULER_SIZE; i++) {
            init(_tasks[i].slotID);
        }

    }

    void NAMFScheduler::getConfigForms(String &page) {
        String s = F("");
        page += F("<div id='ncf'>");
        LoopEntryType i = EMPTY;
        i++;
        for (; i < NAMF_LOOP_SIZE; i++) {
            String templ = F(
                    "<form method='POST' action='/simple_config?sensor={sensor}' style='width:100%;'>\n"
            );
            templ += F("<hr/><h2>");
            templ += findSlotDescription(i);
            templ += F("</h2>");
            boolean checked =  findSlot(i) >= 0; // check if sensor is enabled
            templ += form_checkbox(F("enabled-{sensor}"), FPSTR(INTL_ENABLE), checked, true);
            templ += F("<br/>");
            if (SimpleScheduler::display(i)) {
                checked = sensorWantsDisplay(i);
                templ += form_checkbox(F("display-{sensor}"), FPSTR(INTL_DISPLAY_NEW), checked, true);
                templ += F("<div class='spacer'></div>");
            }
            //HTML to enable/disable given sensor

            s = SimpleScheduler::selectConfigForm(i);
            templ += F("{body}<input type='submit' value='zapisz'/></form>\n");
            templ.replace(F("{sensor}"), String(i));
            templ.replace(F("{body}"), s);
            page += templ;

        }
        page += F("</div>");
    }

    int NAMFScheduler::unregisterSensor(LoopEntryType slot) {
        int i = findSlot(slot);
        if (i < 0) return 0;
        loopSize--;
        _tasks[i].slotID = EMPTY;
        _tasks[i].process(STOP);

    }

    bool NAMFScheduler::isRegistered(LoopEntryType slot) {
        return findSlot(slot) >= 0;
    }

    //inform scheduler that we want to display data on LCD
    int NAMFScheduler::registerDisplay(LoopEntryType slot) {
        int i = findSlot(slot);
        if (i < 0) return -1;
        if (!_tasks[i].hasDisplay) {
            //only when first time enabled increase number of sensors with display
            _tasks[i].hasDisplay = true;
            sensorsWithDisplays++;
        }
    }

    //register new sensor
    int NAMFScheduler::registerSensor(LoopEntryType slot, loopTimerFunc processF, const __FlashStringHelper *code) {
        {
            if (loopSize + 1 >= SCHEDULER_SIZE)
                return -1;
            int i = findSlot(slot);
            //no sensor yet
            if (i < 0) i = findSlot(EMPTY);
            if (i < 0) { return -1; };
            _tasks[i].nextRun = 0;
            _tasks[i].process = processF;
            _tasks[i].slotID = slot;
            _tasks[i].slotCode = code;

            loopSize += 1;
            //return idx
            return loopSize - 1;
        };
    }

//find scheduler entry based on sensor type (slot ID)
    int NAMFScheduler::findSlot(byte id) {
        for (byte i = 0; i < SCHEDULER_SIZE; i++) {
            if (_tasks[i].slotID == id)
                return i;
        }
        //no match
        return -1;

    }

    bool NAMFScheduler::sensorWantsDisplay(LoopEntryType sensor) {
        int i = findSlot(sensor);
        if (i<0) return false;  //sensor is not registered at all
        return _tasks[i].hasDisplay;
    }




    void NAMFScheduler::runIn(byte slot, unsigned long time, loopTimerFunc func) {
        int idx;
        idx = findSlot(slot);
        if (idx < 0) return;

        if (time > 0) {
            _tasks[idx].nextRun = millis() + time;
        } else {
            _tasks[idx].nextRun = 0;
        }
        _tasks[idx].process = func;
    };

    void NAMFScheduler::runIn(byte slot, unsigned long time) {
        int idx;
        idx = findSlot(slot);
        if (idx < 0) return;

        if (time > 0) {
            _tasks[idx].nextRun = millis() + time;
        } else {
            _tasks[idx].nextRun = 0;
        }

    }

    LoopEntryType operator++(LoopEntryType &entry, int) {
        LoopEntryType current = entry;
        if (NAMF_LOOP_SIZE < entry + 1) entry = NAMF_LOOP_SIZE;
        else entry = static_cast<LoopEntryType>( entry + 1);
        return (current);
    }

}
