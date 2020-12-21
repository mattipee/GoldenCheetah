/*
 * Copyright (c) 2011 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRAfNTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "FortiusController.h"
#include "Fortius.h"
#include "RealtimeData.h"

FortiusController::FortiusController(TrainSidebar *parent,  DeviceConfiguration *dc) : RealtimeController(parent, dc)
{
    myFortius = new Fortius (parent);
}


int
FortiusController::start()
{
    return myFortius->start();
}


int
FortiusController::restart()
{
    return myFortius->restart();
}


int
FortiusController::pause()
{
    return myFortius->pause();
}


int
FortiusController::stop()
{
    return myFortius->stop();
}

bool
FortiusController::find()
{
    return myFortius->find(); // needs to find either unconfigured or configured device
}

bool
FortiusController::discover(QString) {return false; } // NOT IMPLEMENTED YET


bool FortiusController::doesPush() { return false; }
bool FortiusController::doesPull() { return true; }
bool FortiusController::doesLoad() { return true; }

/*
 * gets called from the GUI to get updated telemetry.
 * so whilst we are at it we check button status too and
 * act accordingly.
 *
 */
void
FortiusController::getRealtimeData(RealtimeData &rtData)
{
    // Added Distance and Steering here but yet to RealtimeData

    int Buttons, Status, Steering;
    double Power, Resistance, HeartRate, Cadence, Speed, Distance;

    if(!myFortius->isRunning())
    {
        emit setNotification(tr("Cannot Connect to Fortius"), 2);
        parent->Stop(1);
        return;
    }
    // get latest telemetry
    myFortius->getTelemetry(Power, Resistance, HeartRate, Cadence, Speed, Distance, Buttons, Steering, Status);

    //
    // PASS BACK TELEMETRY
    //
    rtData.setWatts(Power);
    rtData.setHr(HeartRate);
    rtData.setCadence(Cadence);
    rtData.setSpeed(Speed);


    // post processing, probably not used
    // since its used to compute power for
    // non-power devices, but we may add other
    // calculations later that might apply
    // means we could calculate power based
    // upon speed even for a Fortius!
    processRealtimeData(rtData);

    //
    // BUTTONS
    //

    // ignore other buttons if calibrating
    if (parent->calibrating) return;

    // ADJUST LOAD
    if ((Buttons&FT_PLUS)) parent->Higher();

    if ((Buttons&FT_MINUS)) parent->Lower();

    // LAP/INTERVAL
    if (Buttons&FT_ENTER) parent->newLap();

    // CANCEL
    if (Buttons&FT_CANCEL) parent->Stop(0);

    // Ensure we set the UI load to the actual setpoint from the fortius (as it will clamp)
    rtData.setLoad(myFortius->getLoad());
    rtData.setSlope(myFortius->getGradient());
}

void FortiusController::pushRealtimeData(RealtimeData &) { } // update realtime data with current values

void
FortiusController::setLoad(double load)
{
    myFortius->setLoad(load);
}

void
FortiusController::setGradient(double grade, double resistanceWatts)
{
    myFortius->setGradient(grade, resistanceWatts);
}

void
FortiusController::setMode(int mode)
{
    if (mode == RT_MODE_ERGO) mode = FT_ERGOMODE;
    else if (mode == RT_MODE_SPIN) mode = FT_SSMODE;
    else mode = FT_IDLE;

    myFortius->setMode(mode);
}

void
FortiusController::setWeight(double weight)
{
    myFortius->setWeight(weight);
}

// TODO check my use of state machine

uint8_t
FortiusController::getCalibrationType()
{
    return CALIBRATION_TYPE_ZERO_OFFSET;
}

double
FortiusController::getCalibrationTargetSpeed()
{
    return 20;
}

uint8_t
FortiusController::getCalibrationState()
{
    return calibrationState;
}

void
FortiusController::setCalibrationState(uint8_t state)
{
    calibrationState = state;
    if (calibrationState == CALIBRATION_STATE_PENDING)
    {
        myFortius->setMode(FT_CALIBRATE);
        calibrationState = CALIBRATION_STATE_STARTING;
    }
    return;
}

uint16_t
FortiusController::getCalibrationZeroOffset()
{
    // TODO is this the right place to implement the stabilisation check?
    switch (calibrationState)
    {
        case CALIBRATION_STATE_STARTING:
        {
            myFortius->setMode(FT_CALIBRATE);
            if (readCurrentSpeedValue() > 0.0)
            {
                calibrationState = CALIBRATION_STATE_STARTED;
            }
        }
        break;

        case CALIBRATION_STATE_STARTED:
        {
            // FIXME ... a static loop counter and 100 iterations is OBVIOUSLY wrong
            // TODO implement stabilisation check (eg threshold on the min/max of most recent 20 values)

            double calVal = readCurrentCalibrationValue();

            static int c=0;
            if (++c > 100)//readCurrentSpeedValue() > 19.9)
            {
                calibrationState = CALIBRATION_STATE_SUCCESS;
                myFortius->setMode(FT_IDLE);
                myFortius->setCalibrationValue(calVal);
            }
        }
        break;

        default: break;
    }
    return 0;
}

void
FortiusController::resetCalibrationState()
{
    calibrationState = CALIBRATION_STATE_IDLE;
    myFortius->setMode(FT_IDLE);

    return;
}

// FIXME dirty pair of functions - refactor
double
FortiusController::readCurrentCalibrationValue()
{
    double power, resistance, heartrate, cadence, speed, distance;
    int buttons, steering, status;
    myFortius->getTelemetry(power, resistance, heartrate, cadence, speed, distance, buttons, steering, status);
    return resistance;
}

double
FortiusController::readCurrentSpeedValue()
{
    double power, resistance, heartrate, cadence, speed, distance;
    int buttons, steering, status;
    myFortius->getTelemetry(power, resistance, heartrate, cadence, speed, distance, buttons, steering, status);
    return speed;
}
