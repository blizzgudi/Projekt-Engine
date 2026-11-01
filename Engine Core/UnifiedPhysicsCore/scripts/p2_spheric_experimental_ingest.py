"""P2 research: SHA-pinned official 3D MARIN/SPHERIC Test-02 workbook ingestion.

This script verifies experimental data integrity. It does NOT run CFD and
must NEVER be used as proof of full P2 fluid validation or acceptance.
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
from datetime import datetime,timezone
from pathlib import Path
import sys
import zipfile

CORE=Path(__file__).resolve().parents[1]
REF=CORE/"references"
RUNS=CORE.parents[1]/"ENGINE TEST STUBE"/"runs"
SOURCE=REF/"SPHERIC_Test02_original.zip"
ORIGINAL_SHA="a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1"
XLS_SHA="8126f8c8f6effb9e8bfb245a2d098186e9ebee2ef6e75fcb1b3166af17883d8c"
HEADERS=["time_s"]+[f"P{i}_Pa" for i in range(1,9)]+[f"H{i}_m" for i in range(1,5)]
COLUMNS=list(range(9))+list(range(10,14))
SAMPLE_TIMES=[0.25,0.5,1.0,1.5,2.0,4.0,6.0]

def sha(data:bytes)->str: return hashlib.sha256(data).hexdigest()

def read_measurements()->list[tuple[float,...]]:
    if not SOURCE.is_file():
        raise RuntimeError("SPHERIC original ZIP absent; fetch from the cited official source")
    raw=SOURCE.read_bytes()
    if sha(raw)!=ORIGINAL_SHA:
        raise RuntimeError("SPHERIC original ZIP SHA256 mismatch; stop rather than repair")
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        names=archive.namelist()
        if sorted(names)!=sorted([
            "test_case_2_exp_data.xls",
            "test_case_2_v1p1.pdf",
            "test_case_2.f"]):
            raise RuntimeError("Unexpected SPHERIC archive layout")
        data=archive.read("test_case_2_exp_data.xls")
    if sha(data)!=XLS_SHA:
        raise RuntimeError("SPHERIC original workbook SHA256 mismatch")
    sys.path.insert(0,str(REF/"vendor"))
    try: import xlrd
    except ImportError as error:
        raise RuntimeError("Missing locally vendored xlrd 2.0.2; install under references/vendor") from error
    book=xlrd.open_workbook(file_contents=data,on_demand=True)
    if book.nsheets!=1:
        raise RuntimeError("Unexpected SPHERIC worksheet count")
    sheet=book.sheet_by_index(0)
    if sheet.name!="Experimental_data" or sheet.nrows!=7396 or sheet.ncols!=14:
        raise RuntimeError("SPHERIC worksheet shape mismatch")
    actual_headers=[str(sheet.cell_value(0,col)).strip() for col in COLUMNS]
    expected=["Time (s)"]+[f"P{i} (Pa)" for i in range(1,9)]+[
        f"H{i} (m)" for i in range(1,5)]
    if actual_headers!=expected:
        raise RuntimeError(f"Unexpected SPHERIC channels: {actual_headers}")
    rows=[]
    last_t=-math.inf
    for row in range(1,sheet.nrows):
        values=tuple(float(sheet.cell_value(row,col)) for col in COLUMNS)
        if len(values)!=13 or any(not math.isfinite(v) for v in values):
            raise RuntimeError(f"Invalid SPHERIC numeric data at row {row}")
        if not values[0]>last_t:
            raise RuntimeError(f"SPHERIC time not strictly increasing at row {row}")
        if any(v < 0 or v > 2 for v in values[9:]):
            raise RuntimeError(f"Unphysical experiment height at row {row}")
        rows.append(values)
        last_t=values[0]
    if len(rows)!=7395 or abs(rows[-1][0]-7.394063535642303)>1e-10:
        raise RuntimeError("SPHERIC measurements truncated")
    return rows

def interpolate(rows:list[tuple[float,...]],time_s:float)->list[float]:
    if time_s<rows[0][0] or time_s>rows[-1][0]:
        raise ValueError("Requested SPHERIC time out of range")
    lo=0;hi=len(rows)-1
    while lo<hi:
        mid=(lo+hi)//2
        if rows[mid][0]<time_s:lo=mid+1
        else:hi=mid
    if lo==0:return list(rows[0][1:])
    a,b=rows[lo-1],rows[lo]
    weight=(time_s-a[0])/(b[0]-a[0])
    return [a[j]+weight*(b[j]-a[j]) for j in range(1,len(a))]

def main():
    rows=read_measurements()
    with (REF/"SPHERIC_Test02_measured.csv").open("w",encoding="utf-8",newline="") as out:
        writer=csv.writer(out,lineterminator="\n")
        writer.writerow(HEADERS)
        writer.writerows(rows)
    sampled={}
    for t in SAMPLE_TIMES:
        vals=interpolate(rows,t)
        sampled[str(t)]={name:val for name,val in zip(HEADERS[1:],vals)}
    diagnostics={}
    for index,name in enumerate(HEADERS[1:],1):
        series=[row[index] for row in rows]
        diagnostics[name]={"min":min(series),"max":max(series),
                           "initial":series[0],"final":series[-1]}
    checks={
        "source_zip_sha256":sha(SOURCE.read_bytes())==ORIGINAL_SHA,
        "exact_7395_rows":len(rows)==7395,
        "all_12_channels":len(diagnostics)==12,
        "monotonic_time":all(a[0]<b[0] for a,b in zip(rows,rows[1:])),
        "physical_initial_reservoir_H4":0.54<rows[0][-1]<0.56,
        "initial_downstream_height_near_zero":all(
            rows[0][i]<0.002 for i in [9,10,11]),
        "records_pass_6_seconds":rows[-1][0]>6,
        "pressure_signs_preserved_not_clipped":any(
            v<0 for row in rows[:100] for v in row[1:9]),
    }
    verified=all(checks.values())
    report={
        "phase":"P2",
        "milestone":"spheric_marin_test02_measured_data_ingestion",
        "source_url":"https://www.spheric-sph.org/tests/test-02",
        "source_archive_url":"https://9449af45-2363-44f6-8b03-03b6b7c2aee5.usrfiles.com/archives/9449af_6ce73873d6d04315853e481c337dbfca.zip",
        "source_zip_sha256":ORIGINAL_SHA,
        "source_workbook_sha256":XLS_SHA,
        "measurements_csv_sha256":sha((REF/"SPHERIC_Test02_measured.csv").read_bytes()),
        "rows":len(rows),
        "channels":HEADERS,
        "time_range_s":[rows[0][0],rows[-1][0]],
        "per_channel":diagnostics,
        "reference_samples":sampled,
        "checks":checks,
        "data_integrity_accepted":verified,
        "compared_with_upc":False,
        "3d_experimental_cfd_validated":False,
        "phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "limits":"Original MARIN/SPHERIC laboratory measurements verified and preserved. "
                 "No simulated gauge/pressure signals compared; therefore not CFD validation.",
    }
    RUNS.mkdir(exist_ok=True)
    output=RUNS/"p2_spheric_marin_measured_ingestion.json"
    output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(f"SPHERIC_DATA_INTEGRITY={verified} ROWS={len(rows)} CHANNELS=12 CFD_VALIDATED=False")
    if not verified:raise SystemExit(1)

if __name__=="__main__":
    main()
