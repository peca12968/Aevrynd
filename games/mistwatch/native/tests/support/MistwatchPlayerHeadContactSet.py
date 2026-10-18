"""Additional per-case contact-hit regression, separate from the unchanged original reader."""

def ValidateHeadContactSet(actual,expected):
    if expected['schema']!='mistwatch.player-head.expected-hit-set.1':raise RuntimeError('HEAD_HIT_EXPECTED_SCHEMA')
    cases=expected['cases'];expected_ids=[case['id']for case in cases];actual_ids=[case['id']for case in actual]
    if len(cases)!=56 or len(set(expected_ids))!=56 or len(actual)!=56 or len(set(actual_ids))!=56 or set(actual_ids)!=set(expected_ids):
        raise RuntimeError('HEAD_HIT_EXACT_56_UNIQUE_CASE_IDS_REQUIRED')
    if sum(case['expected_guarded']for case in cases)!=38:raise RuntimeError('HEAD_HIT_SEALED_38_18_SET_REQUIRED')
    by_id={case['id']:case for case in actual}
    for case in cases:
        observed=by_id[case['id']]
        hit=observed['guarded_events']>0
        if hit!=case['expected_guarded']:
            raise RuntimeError('HEAD_GUARD_HIT_SET_FIRST_DIVERGENCE case='+case['id']+
                ' expected_guarded='+str(int(case['expected_guarded']))+' actual_guarded='+str(int(hit)))
    return dict(cases=56,guarded_hit_cases=38,no_guarded_hit_cases=18,per_case_identity_set_equal=True)
