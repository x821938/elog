#ifndef ELOG_MACROS_H
#define ELOG_MACROS_H

/** Macros for logging */

/** debug
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define debug(logId, message, ...) logger.log(logId, ELOG_LEVEL_DEBUG, message, ##__VA_ARGS__)

/** info
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define info(logId, message, ...) logger.log(logId, ELOG_LEVEL_INFO, message, ##__VA_ARGS__)

/** notice
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define notice(logId, message, ...) logger.log(logId, ELOG_LEVEL_NOTICE, message, ##__VA_ARGS__)

/** warning
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define warning(logId, message, ...) logger.log(logId, ELOG_LEVEL_WARNING, message, ##__VA_ARGS__)

/** error
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define error(logId, message, ...) logger.log(logId, ELOG_LEVEL_ERROR, message, ##__VA_ARGS__)

/** critical
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define critical(logId, message, ...) logger.log(logId, ELOG_LEVEL_CRITICAL, message, ##__VA_ARGS__)

/** alert
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define alert(logId, message, ...) logger.log(logId, ELOG_LEVEL_ALERT, message, ##__VA_ARGS__)

/** emergency
 * @param logId the log id
 * @param message the message
 * @param ... the arguments
 */
#define emergency(logId, message, ...) logger.log(logId, ELOG_LEVEL_EMERGENCY, message, ##__VA_ARGS__)

#endif // ELOG_MACROS_H
