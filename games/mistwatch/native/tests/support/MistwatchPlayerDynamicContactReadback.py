from pathlib import Path
import hashlib,json,struct

def need(value,reason):
    if not value:raise RuntimeError(reason)

def row(path):
    raw=path.read_bytes();return dict(path=path.as_posix(),bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest().upper())

def ValidatePlayerMatrix(ran,fixed,OUT):
    receipt={}
    text=Path(ran["stdout"]["path"]).read_text(encoding="ascii")
    stderr=Path(ran["stderr"]["path"]).read_text(encoding="ascii")
    receipt['CPU_stdout_pin']=row(Path(ran['stdout']['path']));receipt['CPU_stderr_pin']=row(Path(ran['stderr']['path']))
    need(stderr=='','UNEXPECTED_CPU_STDERR')
    def fields(line):return dict(piece.split('=',1) for piece in line.split()[1:])
    indexed=list(enumerate(text.splitlines(),1))
    def records(prefix):return [(number,fields(line)) for number,line in indexed if line.startswith(prefix+' ')]
    setups=records('SETUP_POSE');begins=records('PHASE_CASE_BEGIN');ends=records('PHASE_CASE_END')
    steps=records('PLAYER_HEAD_STEP');events=records('PHASE_CONTACT');complete=records('PHASE_AUDIT_COMPLETE')
    need(len(setups)==len(begins)==len(ends)==56 and len(complete)==1,'FULL_FIXED_MATRIX_MEMBERSHIP_MISSING')
    need(all(x['accepted']=='1' and x['joints']=='15' and x['vertices']=='9281' and x['indices']=='37032' and x['grounded']=='1' for _,x in setups),'REAL_MODEL_OR_INITIAL_POSE_NOT_ACCEPTED')
    need(len(steps)==56*38 and all(x['accepted']=='1' and x['pose_rejected']=='0' and x['steps']=='1' and x['published_contact_valid']=='1' for _,x in steps),'FULL_ACTUAL_FIXED_STEPS_MISSING')
    f32=lambda value:struct.unpack('<f',struct.pack('<f',value))[0]
    bits=lambda value:struct.unpack('<I',struct.pack('<f',value))[0]
    fixedH=struct.unpack('<f',struct.pack('<I',fixed['shared_recipe']['FixedStep_f32_bits']))[0]
    matrix=[];computed_events=[];noAbove=0;violationCases=0;inferred={0:0,1:0,2:0}
    for i,case in enumerate(fixed['cases']):
        beginLine,begin=begins[i];endLine,end=ends[i];setupLine,setup=setups[i]
        need(int(begin['index'])==int(end['index'])==i and int(begin['millimeters'])==int(end['millimeters'])==case['target_millimeters'],'CASE_ORDER_OR_ID_CHANGED')
        need(int(begin['bottom_bits'])==case['bottom_f32_bits'] and int(begin['center_y_bits'])==case['body_center_y_f32_bits'] and begin['zero_query_clear']=='1','CASE_RAW_GEOMETRY_OR_INITIAL_QUERY_CHANGED')
        own=[(line,x) for line,x in steps if x['case']==case['id']]
        own_events=[(line,x) for line,x in events if int(x['index'])==i]
        need([int(x['tick']) for _,x in own]==list(range(38)) and setupLine<beginLine<own[0][0]<own[-1][0]<endLine,'CASE_STEP_SEQUENCE_CHANGED')
        need(all(beginLine<line<endLine for line,_ in own_events),'CONTACT_OUTSIDE_CASE')
        guarded=[(line,x) for line,x in own if x['above']=='1' and float(x['before_vy'])>0]
        need(len(guarded)==len(own_events) and len({x['tick'] for _,x in own_events})==len(own_events),'GUARDED_EVENT_SET_CHANGED')
        actualEvents=[]
        for (stepLine,step),(eventLine,event) in zip(guarded,own_events):
            need(event['tick']==step['tick'] and stepLine<eventLine and event['millimeters']==begin['millimeters'] and event['before_vy']==step['before_vy'] and event['after_vy']==step['vy'],'EVENT_NOT_ACTUAL_STEP_OUTPUT')
            expectedCd=f32(max(0.,f32(float(step['before_cooldown']))-fixedH))
            requireRemaining=float(step['remaining'])==0
            requireCooldown=bits(float(step['cooldown']))==bits(expectedCd)
            requireJump=step['before_jumps']==step['jumps'];requireLanding=step['before_landings']==step['landings']
            requireGround=step['grounded']=='0'
            violation=not all((requireRemaining,requireCooldown,requireJump,requireLanding,requireGround))
            for field,value in (('remaining_consumed_once',requireRemaining),('cooldown_natural',requireCooldown),
                ('no_added_jump',requireJump),('no_added_landing',requireLanding),('no_fake_ground',requireGround),('invariant_violation',violation)):
                need(event[field]==str(int(value)),'CONTACT_REQUIREMENT_READBACK_DISAGREES:'+field)
            phase=0
            if begin['inner_count']=='2' and step['below']=='0' and step['side']=='0':
                if bits(float(step['vy']))==bits(f32(-20*f32(float(begin['inner_h'])))):phase=1
                elif float(step['vy'])==0:phase=2
            need(int(event['source_inferred_inner_phase'])==phase and event['inference_not_internal_instrumentation']=='1' and event['split_step_equivalence_claim']=='0','CONTACT_PHASE_SCOPE_OR_READBACK_CHANGED')
            inferred[phase]+=1
            actualEvents.append(dict(tick=int(step['tick']),raw_step_line=stepLine,raw_event_line=eventLine,step=step,event=event,
                independently_readback_violation=violation,source_inferred_phase_only=phase))
        above=any(x['above']=='1' for _,x in own);violations=sum(x['independently_readback_violation'] for x in actualEvents)
        need(int(end['completed_ticks'])==38 and end['above_seen']==str(int(above)) and int(end['guarded_events'])==len(guarded) and
            int(end['violating_events'])==violations and end['hypothesis_violated']==str(int(violations>0)),'CASE_SUMMARY_READBACK_DIFFERS')
        if not above:noAbove+=1
        if violations:violationCases+=1
        matrix.append(dict(index=i,id=case['id'],target_millimeters=case['target_millimeters'],status='FULL38_COMPLETED',query_clear=True,
            setup_pose_accepted=True,completed_ticks=38,above_seen=above,guarded_events=len(guarded),violating_events=violations,
            hypothesis_violated=violations>0,raw_setup_line=setupLine,raw_case_begin_line=beginLine,raw_case_end_line=endLine,
            raw_step_line_span=[own[0][0],own[-1][0]],raw_stdout=receipt['CPU_stdout_pin'],contacts=actualEvents))
        computed_events+=actualEvents
    summary=complete[0][1];violationCount=sum(x['independently_readback_violation'] for x in computed_events)
    actualCounts=dict(cases=56,query_clear_cases=56,guarded_events=len(computed_events),violating_events=violationCount,
        violating_cases=violationCases,no_above_cases=noAbove,source_inferred_first=inferred[1],source_inferred_second=inferred[2],unclassified=inferred[0])
    need(all(int(summary[key])==value for key,value in actualCounts.items()) and summary['all_fixed_cases_retained']=='1','FINAL_MATRIX_COUNT_DIFFERS')
    need(ran['exit_code']==(1 if violationCount else 0),'PROPERTY_EXIT_DOES_NOT_MATCH_FULL_MATRIX')
    matrixPath=OUT/'matrix.json'
    with matrixPath.open('x',encoding='utf-8') as stream:json.dump(dict(schema='mistwatch.player-head.phase-matrix.1',summary=actualCounts,cases=matrix),stream,indent=2);stream.write('\n')
    receipt['matrix']=row(matrixPath);receipt['actual_counts']=actualCounts;receipt['case_actuals']=matrix
    receipt['phase_scope']='Source-inferred from full actual call terminalvy only; not internal instrumentation or split-state equivalence.'
    need(violationCount==0 and ran['exit_code']==0,'MATCHED_GREEN_PROPERTY_STILL_VIOLATED')
    return receipt
