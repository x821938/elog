#ifndef ELOG_MACROS_H
#define ELOG_MACROS_H

/** Macros for logging */

/** debug
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define debug(logId, message, ...) logger.log(logId, DEBUG, message, ##__VA_ARGS__)

/** info
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define info(logId, message, ...) logger.log(logId, INFO, message, ##__VA_ARGS__)

/** notice
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define notice(logId, message, ...) logger.log(logId, NOTICE, message, ##__VA_ARGS__)

/** warning
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define warning(logId, message, ...) logger.log(logId, WARNING, message, ##__VA_ARGS__)

/** error
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define error(logId, message, ...) logger.log(logId, ERROR, message, ##__VA_ARGS__)

/** critical
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define critical(logId, message, ...) logger.log(logId, CRITICAL, message, ##__VA_ARGS__)

/** alert
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define alert(logId, message, ...) logger.log(logId, ALERT, message, ##__VA_ARGS__)

/** emergency
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define emergency(logId, message, ...) logger.log(logId, EMERGENCY, message, ##__VA_ARGS__)

#endif // ELOG_MACROS_H