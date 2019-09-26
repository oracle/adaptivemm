#include "predict.h"

#ifndef	PREDICT_IMPL_H
#define	PREDICT_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#define	SUM_X	((LSQ_LOOKBACK * (1 - LSQ_LOOKBACK)) / 2)
#define	SUM_XX	((LSQ_LOOKBACK * (LSQ_LOOKBACK - 1) * (2 * LSQ_LOOKBACK - 1)) / 6)

#ifdef __cplusplus
}
#endif

#endif /* PREDICT_IMPL_H */
