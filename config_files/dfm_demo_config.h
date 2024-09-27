/*
 * demo_config.h
 *
 *  Created on: Jul 25, 2024
 *      Author: johan
 */

#ifndef DEVALERTDEMO_DEMO_CONFIG_H_
#define DEVALERTDEMO_DEMO_CONFIG_H_


/**
 * @brief Flag for selecting the demo mode. The modes are:
 *
 * 0: The demo application will connect to your AWS account (AWS IoT Core/MQTT) over Wifi to upload the Alerts that way.
 * This requires a DevAlert deployment in your AWS account and a AWS-enabled DevAlert account. Contact support@percepio.com
 * if you want to try this.
 *
 * 1: When enabled, this mode will transmit the Alert data to the host computer using the serial port of the integrated STLINK debugger.
 * This is intended for initial evaluation of DevAlert, without requiring cloud connectivity in your device.
 * Host-side tools are available for uploading the data to your DevAlert cloud storage, either an Amazon S3 bucket or a DevAlert evaluation account.
 *
 * In the SW4STM32 demo project, this flag is defined in the project settings for each build configuration.
 */
#ifndef DEMO_CFG_SERIAL_UPLOAD_ONLY
#define DEMO_CFG_SERIAL_UPLOAD_ONLY 1
#endif

/**
 * @brief Flag for selecting the Core Dump format. The modes are:
 *
 * 0: The demo application will use crashcatcher for core dumps.
 *
 * 1: The Zephyr core dump format is used.
 *
 * Note that DFM does not use this setting internally. This is only used in the demo code, e.g. to select the right fault handler implementation.
 * But the setting is closely related to DFM, so it ended up here.
 **/
#ifndef DEMO_CFG_USE_ZEPHYR_CORE_DUMP_FORMAT
#define DEMO_CFG_USE_ZEPHYR_CORE_DUMP_FORMAT 0
#endif

#endif /* DEVALERTDEMO_DEMO_CONFIG_H_ */
