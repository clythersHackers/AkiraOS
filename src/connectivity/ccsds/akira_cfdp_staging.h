/**
 * @file akira_cfdp_staging.h
 * @brief Akira-facing CFDP staging path wrapper.
 */

#ifndef AKIRA_CCSDS_CFDP_STAGING_H
#define AKIRA_CCSDS_CFDP_STAGING_H

#ifdef __cplusplus
extern "C" {
#endif

int akira_cfdp_receive_to_staging(const char *dst_name);
int akira_cfdp_commit_staged(const char *dst_name);
int akira_cfdp_discard_staged(const char *dst_name);

#ifdef CONFIG_ZTEST
struct akira_cfdp_staging_ops {
    void *user;
    int (*mkdir)(void *user, const char *path);
    int (*create)(void *user, const char *path);
    int (*rename)(void *user, const char *from, const char *to);
    int (*unlink)(void *user, const char *path);
};

void akira_cfdp_staging_set_ops_for_test(
    const struct akira_cfdp_staging_ops *ops);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_CFDP_STAGING_H */
