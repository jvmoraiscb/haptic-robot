#include <falcon/core/FalconDevice.h>
#include <falcon/firmware/FalconFirmwareNovintSDK.h>
#include <falcon/grip/FalconGripFourButton.h>
#include <falcon/kinematic/FalconKinematicStamper.h>
#include <falcon/util/FalconFirmwareBinaryNvent.h>

#include <cmath>
#include <iostream>
#include <string>

using namespace libnifalcon;
using namespace std;
using namespace StamperKinematicImpl;

/// Ask libnifalcon to get the Falcon ready for action
/// nothing clever here, straight from the examples
bool initialise(FalconDevice* falcon) {
    falcon->setFalconFirmware<FalconFirmwareNovintSDK>();

    cout << "Setting up comm interface for Falcon comms" << endl;

    unsigned int count;
    falcon->getDeviceCount(count);
    cout << "Connected Device Count: " << count << endl;

    // Open the device number:
    int deviceNum = 0;
    cout << "Attempting to open Falcon device:  " << deviceNum << endl;
    if (!falcon->open(deviceNum)) {
        cout << "Cannot open falcon device index " << deviceNum << " - Lib Error Code: " << falcon->getErrorCode() << " Device Error Code: " << falcon->getFalconComm()->getDeviceErrorCode() << endl;
        return false;
    } else {
        cout << "Connected to Falcon device " << deviceNum << endl;
    }

    // Load the device firmware:
    // There's only one kind of firmware right now, so automatically set that.
    falcon->setFalconFirmware<FalconFirmwareNovintSDK>();
    // Next load the firmware to the device

    bool skip_checksum = false;
    // See if we have firmware
    bool firmware_loaded = false;
    firmware_loaded = falcon->isFirmwareLoaded();
    if (!firmware_loaded) {
        std::cout << "Loading firmware" << std::endl;
        uint8_t* firmware_block;
        long firmware_size;
        {
            firmware_block = const_cast<uint8_t*>(NOVINT_FALCON_NVENT_FIRMWARE);
            firmware_size = NOVINT_FALCON_NVENT_FIRMWARE_SIZE;

            for (int i = 0; i < 10; ++i) {
                if (!falcon->getFalconFirmware()->loadFirmware(skip_checksum, NOVINT_FALCON_NVENT_FIRMWARE_SIZE, const_cast<uint8_t*>(NOVINT_FALCON_NVENT_FIRMWARE)))

                {
                    cout << "Firmware loading try failed";
                    // Completely close and reopen
                    // falcon.close();
                    // if(!falcon.open(m_varMap["device_index"].as<int>()))
                    //{
                    //	std::cout << "Cannot open falcon device index " << m_varMap["device_index"].as<int>() << " - Lib Error Code: " << m_falconDevice->getErrorCode() << " Device Error Code: " << m_falconDevice->getFalconComm()->getDeviceErrorCode() << std::endl;
                    //	return false;
                    // }
                } else {
                    firmware_loaded = true;
                    break;
                }
            }
        }
    } else if (!firmware_loaded) {
        std::cout << "No firmware loaded to device, and no firmware specified to load (--nvent_firmware, --test_firmware, etc...). Cannot continue" << std::endl;
        // return false;
    } else {
        // return true;
    }
    if (!firmware_loaded || !falcon->isFirmwareLoaded()) {
        std::cout << "No firmware loaded to device, cannot continue" << std::endl;
        // return false;
    }
    std::cout << "Firmware loaded" << std::endl;

    // Seems to be important to run the io loop once to be sure of sensible values next time:
    falcon->runIOLoop();

    // falcon.getFalconFirmware()->setHomingMode(true);
    falcon->setFalconKinematic<libnifalcon::FalconKinematicStamper>();
    falcon->setFalconGrip<libnifalcon::FalconGripFourButton>();

    return true;
}

/// The velocity Jacobian where Vel=J*theta and Torque=J'*Force
/// Derivation in a slightly different style to Stamper
/// and may result in a couple of sign changes due to the configuration
/// of the Falcon
gmtl::Matrix33d jacobian(const Angle& angles) {
    // Naming scheme:
    // Jx1 = rotational velocity of joint 1 due to linear velocity in x

    gmtl::Matrix33d J;

    // Arm1:
    double den = -libnifalcon::a * sin(angles.theta3[0]) * (sin(angles.theta1[0]) * cos(angles.theta2[0]) - sin(angles.theta2[0]) * cos(angles.theta1[0]));

    double Jx0 = cos(phy[0]) * cos(angles.theta2[0]) * sin(angles.theta3[0]) / den - sin(phy[0]) * cos(angles.theta3[0]) / den;
    double Jy0 = sin(phy[0]) * cos(angles.theta2[0]) * sin(angles.theta3[0]) / den + cos(phy[0]) * cos(angles.theta3[0]) / den;
    double Jz0 = (sin(angles.theta2[0]) * sin(angles.theta2[0])) / (den);

    // Arm2:
    den = -libnifalcon::a * sin(angles.theta3[1]) * (sin(angles.theta1[1]) * cos(angles.theta2[1]) - sin(angles.theta2[1]) * cos(angles.theta1[1]));

    double Jx1 = cos(phy[1]) * cos(angles.theta2[1]) * sin(angles.theta3[1]) / den - sin(phy[1]) * cos(angles.theta3[1]) / den;
    double Jy1 = sin(phy[1]) * cos(angles.theta2[1]) * sin(angles.theta3[1]) / den + cos(phy[1]) * cos(angles.theta3[1]) / den;
    double Jz1 = (sin(angles.theta2[1]) * sin(angles.theta2[1])) / (den);

    // Arm3:
    den = -libnifalcon::a * sin(angles.theta3[2]) * (sin(angles.theta1[2]) * cos(angles.theta2[2]) - sin(angles.theta2[2]) * cos(angles.theta1[2]));

    double Jx2 = cos(phy[2]) * cos(angles.theta2[2]) * sin(angles.theta3[2]) / den - sin(phy[2]) * cos(angles.theta3[2]) / den;
    double Jy2 = sin(phy[2]) * cos(angles.theta2[2]) * sin(angles.theta3[2]) / den + cos(phy[2]) * cos(angles.theta3[2]) / den;
    double Jz2 = (sin(angles.theta2[2]) * sin(angles.theta2[2])) / (den);

    J(0, 0) = Jx0;
    J(0, 1) = Jy0;
    J(0, 2) = Jz0;
    J(1, 0) = Jx1;
    J(1, 1) = Jy1;
    J(1, 2) = Jz1;
    J(2, 0) = Jx2;
    J(2, 1) = Jy2;
    J(2, 2) = Jz2;

    J.setState(J.FULL);
    invert(J);

    // ToDo: Check to see if Jacobian inverted properly.
    // If not we need to take action.

    return J;
}

// Inverse kinematics. All as in Stamper's PhD except for
// the addition of a second offset direction 's' per arm
void IK(Angle& angles, const gmtl::Vec3d& worldPosition) {
    // First we need the offset vector from the origin of the XYZ coordinate frame to the
    // UVW coordinate frame:
    gmtl::Vec3d offset(-libnifalcon::r, -libnifalcon::s, 0);

    // Next lets convert the current end effector position into the UVW coordinates
    // of each leg:
    gmtl::Matrix33d R;
    R(0, 0) = cos(libnifalcon::phy[0]);
    R(0, 1) = sin(libnifalcon::phy[0]);
    R(0, 2) = 0;
    R(1, 0) = -sin(libnifalcon::phy[0]);
    R(1, 1) = cos(libnifalcon::phy[0]);
    R(1, 2) = 0;
    R(2, 0) = 0;
    R(2, 1) = 0;
    R(2, 2) = 1;
    gmtl::Vec3d P1 = R * worldPosition + offset;

    R(0, 0) = cos(libnifalcon::phy[1]);
    R(0, 1) = sin(libnifalcon::phy[1]);
    R(0, 2) = 0;
    R(1, 0) = -sin(libnifalcon::phy[1]);
    R(1, 1) = cos(libnifalcon::phy[1]);
    R(1, 2) = 0;
    R(2, 0) = 0;
    R(2, 1) = 0;
    R(2, 2) = 1;
    gmtl::Vec3d P2 = R * worldPosition + offset;

    R(0, 0) = cos(libnifalcon::phy[2]);
    R(0, 1) = sin(libnifalcon::phy[2]);
    R(0, 2) = 0;
    R(1, 0) = -sin(libnifalcon::phy[2]);
    R(1, 1) = cos(libnifalcon::phy[2]);
    R(1, 2) = 0;
    R(2, 0) = 0;
    R(2, 1) = 0;
    R(2, 2) = 1;
    gmtl::Vec3d P3 = R * worldPosition + offset;

    // Do the theta3's first. This is +/- but fortunately in the Falcon's case
    // only the + result is correct
    angles.theta3[0] = acos((P1[1] + libnifalcon::f) / libnifalcon::b);
    angles.theta3[1] = acos((P2[1] + libnifalcon::f) / libnifalcon::b);
    angles.theta3[2] = acos((P3[1] + libnifalcon::f) / libnifalcon::b);

    // Next find the theta1's
    // In certain cases could query the theta1 values directly and save a bit of processing
    // Again we have a +/- situation but only + is relevent
    double l01 = P1[2] * P1[2] + P1[0] * P1[0] + 2 * libnifalcon::c * P1[0] - 2 * libnifalcon::a * P1[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[0]) * sin(angles.theta3[0]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[0]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[0]) - 2 * libnifalcon::d * libnifalcon::e - 2 * libnifalcon::a * libnifalcon::c;
    double l11 = -4 * libnifalcon::a * P1[2];
    double l21 = P1[2] * P1[2] + P1[0] * P1[0] + 2 * libnifalcon::c * P1[0] + 2 * libnifalcon::a * P1[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[0]) * sin(angles.theta3[0]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[0]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[0]) - 2 * libnifalcon::d * libnifalcon::e + 2 * libnifalcon::a * libnifalcon::c;

    double l02 = P2[2] * P2[2] + P2[0] * P2[0] + 2 * libnifalcon::c * P2[0] - 2 * libnifalcon::a * P2[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[1]) * sin(angles.theta3[1]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[1]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[1]) - 2 * libnifalcon::d * libnifalcon::e - 2 * libnifalcon::a * libnifalcon::c;
    double l12 = -4 * libnifalcon::a * P2[2];
    double l22 = P2[2] * P2[2] + P2[0] * P2[0] + 2 * libnifalcon::c * P2[0] + 2 * libnifalcon::a * P2[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[1]) * sin(angles.theta3[1]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[1]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[1]) - 2 * libnifalcon::d * libnifalcon::e + 2 * libnifalcon::a * libnifalcon::c;

    double l03 = P3[2] * P3[2] + P3[0] * P3[0] + 2 * libnifalcon::c * P3[0] - 2 * libnifalcon::a * P3[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[2]) * sin(angles.theta3[2]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[2]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[2]) - 2 * libnifalcon::d * libnifalcon::e - 2 * libnifalcon::a * libnifalcon::c;
    double l13 = -4 * libnifalcon::a * P3[2];
    double l23 = P3[2] * P3[2] + P3[0] * P3[0] + 2 * libnifalcon::c * P3[0] + 2 * libnifalcon::a * P3[0] + libnifalcon::a * libnifalcon::a + libnifalcon::c * libnifalcon::c - libnifalcon::d * libnifalcon::d - libnifalcon::e * libnifalcon::e - libnifalcon::b * libnifalcon::b * sin(angles.theta3[2]) * sin(angles.theta3[2]) - 2 * libnifalcon::b * libnifalcon::e * sin(angles.theta3[2]) - 2 * libnifalcon::b * libnifalcon::d * sin(angles.theta3[2]) - 2 * libnifalcon::d * libnifalcon::e + 2 * libnifalcon::a * libnifalcon::c;

    double T1a = (-l11 + sqrt(l11 * l11 - 4 * l01 * l21)) / (2 * l21);
    double T2a = (-l12 + sqrt(l12 * l12 - 4 * l02 * l22)) / (2 * l22);
    double T3a = (-l13 + sqrt(l13 * l13 - 4 * l03 * l23)) / (2 * l23);

    double T1b = (-l11 - sqrt(l11 * l11 - 4 * l01 * l21)) / (2 * l21);
    double T2b = (-l12 - sqrt(l12 * l12 - 4 * l02 * l22)) / (2 * l22);
    double T3b = (-l13 - sqrt(l13 * l13 - 4 * l03 * l23)) / (2 * l23);

    angles.theta1[0] = atan(T1b) * 2;
    angles.theta1[1] = atan(T2b) * 2;
    angles.theta1[2] = atan(T3b) * 2;

    // And finally calculate the theta2 values:
    angles.theta2[0] = acos((-P1[0] + libnifalcon::a * cos(angles.theta1[0]) - libnifalcon::c) / (-libnifalcon::d - libnifalcon::e - libnifalcon::b * sin(angles.theta3[0])));
    angles.theta2[1] = acos((-P2[0] + libnifalcon::a * cos(angles.theta1[1]) - libnifalcon::c) / (-libnifalcon::d - libnifalcon::e - libnifalcon::b * sin(angles.theta3[1])));
    angles.theta2[2] = acos((-P3[0] + libnifalcon::a * cos(angles.theta1[2]) - libnifalcon::c) / (-libnifalcon::d - libnifalcon::e - libnifalcon::b * sin(angles.theta3[2])));
}

/// Forward kinematics. Standard Newton-Raphson for linear
/// systems using Jacobian to estimate slope. A small amount
/// of adjustment in the step size is all that is requried
/// to guarentee convergence
void FK(const gmtl::Vec3d& theta0, gmtl::Vec3d& pos) {
    Angle angles;
    gmtl::Vec3d previousPos(pos);
    gmtl::Vec3d currentPos(pos);
    gmtl::Matrix33d J;
    gmtl::Vec3d delta;

    double targetError = 0.01;
    double previousError = 10000.0;
    double gradientAdjustment = 0.5;
    int maxTries = 15;

    bool done = 0;
    for (int i = 0; i < maxTries; i++) {
        // All we have initially are the three values for Theta0 and a guess of position

        // We can use the position guess to generate the angles at this position:
        IK(angles, previousPos);
        // And these angles to find the Jacobian at the current position:
        J = jacobian(angles);
        // Then we can use the Jacobian to tell us which direction we need to move
        // in to rotate each theta0 to towards our desired values

        // Then we can see the difference between the actual and guess theta0:
        delta[0] = theta0[0] - angles.theta1[0];
        delta[1] = theta0[1] - angles.theta1[1];
        delta[2] = theta0[2] - angles.theta1[2];

        // Now use the Jacobian to tell us the direction:
        delta = J * delta;

        // And now we move along the adjustment vector
        // Nb: A good gradient descent algorithm would use more
        // intelligent step size adjustment. Here it only seems
        // to take a couple of steps to converge normally so we
        // simply start with a sensible step size and reduce it
        // if necessary to avoid oscillation about the target error.

        // Take the step size into account:
        delta *= gradientAdjustment;
        // And move the position guess:
        currentPos = previousPos + delta;

        // Let's see if we have got close enough to the target:
        // double error = sqrt(gmtl::dot(delta,delta));
        delta[0] = theta0[0] - angles.theta1[0];
        delta[1] = theta0[1] - angles.theta1[1];
        delta[2] = theta0[2] - angles.theta1[2];
        double error = dot(delta, delta);
        error = sqrt(error);
        previousPos = currentPos;

        if (error < targetError) {
            // Error is low enough so return the current position estimate
            pos = previousPos;
            // cout << i << endl;
            return;
        }
        // Error isn't small enough yet, see if we have over shot
        if ((error > previousError)) {
            // Whoops, over shot, reduce the stepsize next time:
            gradientAdjustment /= 2.0;
        }

        previousError = error;
    }

    // Failed to converge, leave last position as it was
    cout << "Failed to find the tool position in the max tries" << endl;
}

class outputFalcon {
   private:
    double max_x, max_y, max_z;
    double min_x, min_y, min_z;
    double x, y, z;

    bool button1Down, button2Down, button3Down, button4Down;
    int button1, button2, button3, button4;

   public:
    outputFalcon(FalconDevice* falcon) {
        // Ask libnifalcon to update the encoder positions and apply any forces waiting:
        falcon->runIOLoop();

        // Request the current encoder positions:
        std::array<int, 3> encoderPos;
        encoderPos = falcon->getFalconFirmware()->getEncoderValues();
        gmtl::Vec3d encoderAngles;
        encoderAngles[0] = falcon->getFalconKinematic()->getTheta(encoderPos[0]);
        encoderAngles[1] = falcon->getFalconKinematic()->getTheta(encoderPos[1]);
        encoderAngles[2] = falcon->getFalconKinematic()->getTheta(encoderPos[2]);
        encoderAngles *= 0.0174532925;  // Convert to radians

        gmtl::Vec3d pos(0.0, 0.0, 0.11);  // Lets assume the device starts off roughly in the centre of the workspace

        // Forward Kinematics
        FK(encoderAngles, pos);

        double x_temp = pos[0];
        double y_temp = pos[1];
        double z_temp = pos[2];

        // default values
        this->max_x = x_temp;
        this->min_x = x_temp;
        this->max_y = y_temp;
        this->min_y = y_temp;
        this->max_z = z_temp;
        this->min_z = z_temp;

        this->button1Down = false;
        this->button2Down = false;
        this->button3Down = false;
        this->button4Down = false;
        this->button1 = 0;
        this->button2 = 0;
        this->button3 = 0;
        this->button4 = 0;
    }

    void update(FalconDevice* falcon) {
        // Ask libnifalcon to update the encoder positions and apply any forces waiting:
        falcon->runIOLoop();

        // Request the current encoder positions:
        std::array<int, 3> encoderPos;
        encoderPos = falcon->getFalconFirmware()->getEncoderValues();
        gmtl::Vec3d encoderAngles;
        encoderAngles[0] = falcon->getFalconKinematic()->getTheta(encoderPos[0]);
        encoderAngles[1] = falcon->getFalconKinematic()->getTheta(encoderPos[1]);
        encoderAngles[2] = falcon->getFalconKinematic()->getTheta(encoderPos[2]);
        encoderAngles *= 0.0174532925;  // Convert to radians

        gmtl::Vec3d pos(0.0, 0.0, 0.11);  // Lets assume the device starts off roughly in the centre of the workspace

        // Forward Kinematics
        FK(encoderAngles, pos);

        double x_temp = pos[0];
        double y_temp = pos[1];
        double z_temp = pos[2];

        if (x_temp > this->max_x) {
            this->max_x = x_temp;
        }
        if (y_temp > this->max_y) {
            this->max_y = y_temp;
        }
        if (z_temp > this->max_z) {
            this->max_z = z_temp;
        }
        if (x_temp < this->min_x) {
            this->min_x = x_temp;
        }
        if (y_temp < this->min_y) {
            this->min_y = y_temp;
        }
        if (z_temp < this->min_z) {
            this->min_z = z_temp;
        }

        this->x = (x_temp - this->min_x) / (this->max_x - this->min_x);
        this->y = (y_temp - this->min_y) / (this->max_y - this->min_y);
        this->z = (z_temp - this->min_z) / (this->max_z - this->min_z);

        // Cheap debounce
        if (falcon->getFalconGrip()->getDigitalInputs() & libnifalcon::FalconGripFourButton::BUTTON_1) {
            this->button1Down = true;
        } else if (button1Down) {
            if (this->button1 == 0) {
                this->button1 = 1;
            } else {
                this->button1 = 0;
            }
            this->button1Down = false;
        }

        // Cheap debounce
        if (falcon->getFalconGrip()->getDigitalInputs() & libnifalcon::FalconGripFourButton::BUTTON_2) {
            this->button2Down = true;
        } else if (button2Down) {
            if (this->button2 == 0) {
                this->button2 = 1;
            } else {
                this->button2 = 0;
            }
            this->button2Down = false;
        }

        // Cheap debounce
        if (falcon->getFalconGrip()->getDigitalInputs() & libnifalcon::FalconGripFourButton::BUTTON_3) {
            this->button3Down = true;
        } else if (this->button3Down) {
            if (this->button3 == 0) {
                this->button3 = 1;
            } else {
                this->button3 = 0;
            }
            this->button3Down = false;
        }

        // Cheap debounce
        if (falcon->getFalconGrip()->getDigitalInputs() & libnifalcon::FalconGripFourButton::BUTTON_4) {
            this->button4Down = true;
        } else if (this->button4Down) {
            if (button4 == 0) {
                this->button4 = 1;
            } else {
                this->button4 = 0;
            }
            this->button4Down = false;
        }
    }

    void get(double* x, double* y, double* z, int* button1, int* button2, int* button3, int* button4) {
        *x = this->x;
        *y = this->y;
        *z = this->z;
        *button1 = this->button1;
        *button2 = this->button2;
        *button3 = this->button3;
        *button4 = this->button4;
    }
};

int main(int argc, char* argv[]) {
    FalconDevice falcon;

    if (!initialise(&falcon))
        return 1;

    outputFalcon output = outputFalcon(&falcon);

    while (true) {
        double x, y, z;
        int button1, button2, button3, button4;

        output.update(&falcon);
        output.get(&x, &y, &z, &button1, &button2, &button3, &button4);

        printf("X: %.2lf | Y:%.2lf | Z: %.2lf | b1: %d | b2: %d | b3: %d | b4: %d\n", x, y, z, button1, button2, button3, button4);
    }

    return 0;
}