#ifndef __UPGRADE_H__
#define __UPGRADE_H__

void upgrade_init();
void upgrade_run();
int upgrade_app_available();
int upgrade_app_version();
int upgrade_backup_available();
int upgrade_backup_version();

#endif
