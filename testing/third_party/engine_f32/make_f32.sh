#!/bin/bash
# Regenerates the files of this directory from ../engine: the same collision code with floats where oDFe has doubles, which is
# what the ioq3 based DeFRaG engines compute (ioq3's cm_trace.c / cm_patch.c / cm_polylib.c differ from oDFe's in this and in
# nothing that changes a result). Run it again after updating ../engine.
cd "$(dirname "$0")"
for f in cm_trace cm_patch cm_polylib; do
	sed -e 's/\bdouble\b/float/g' -e 's/DotProductDPf/DotProduct/g' -e 's/DotProductDP/DotProduct/g' \
		-e 's/VectorSubtractDP/VectorSubtract/g' -e 's/VectorAddDP/VectorAdd/g' -e 's/CrossProductDP/CrossProduct/g' \
		-e 's/VectorNormalizeDP/VectorNormalize/g' ../engine/$f.c > $f.c
done
