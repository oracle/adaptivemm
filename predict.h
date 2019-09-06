#ifndef PREDICT_H
#define	PREDICT_H

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_ORDER	11
#define	COUNT		8

struct lsq_struct {
	int slot;
	int ready;
	long long y[COUNT];
	long long sum_xy;
	long long sum_y;
};

enum prediction_type { TYPE_NONE, TYPE_ONE, TYPE_TWO };

struct prediction_struct {
	int order;
	enum prediction_type type;
	float f_T_zero;
	float R_T;
	float f_f_zero;
	float R_f;
	float t_e;
	float f_e;
};

struct prediction_struct *predict(unsigned long *, struct lsq_struct *, int,
    int, struct prediction_struct *);

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_H */
