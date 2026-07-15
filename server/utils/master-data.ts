export const employeeDefaults = [
  { id: 'fallback-emp-01', code: 'EMP001', name: 'Nida Boonmee', active: true },
  { id: 'fallback-emp-02', code: 'EMP002', name: 'Kittipong Chaiya', active: true },
  { id: 'fallback-emp-03', code: 'EMP003', name: 'Somchai Rakdee', active: true },
  { id: 'fallback-emp-04', code: 'EMP004', name: 'Anong Srisuk', active: true },
  { id: 'fallback-emp-05', code: 'EMP005', name: 'Teerin Admin', active: true },
] as const

export const salesOrderDefaults = [
  { id: 'fallback-so-01', soNumber: 'SO-240702', description: 'Battery QC Batch SO-240702', active: true },
  { id: 'fallback-so-02', soNumber: 'SO-240703', description: 'Battery QC Batch SO-240703', active: true },
  { id: 'fallback-so-03', soNumber: 'SO-240704', description: 'Battery QC Batch SO-240704', active: true },
  { id: 'fallback-so-04', soNumber: 'SO-240705', description: 'Battery QC Batch SO-240705', active: true },
  { id: 'fallback-so-05', soNumber: 'SO-240706', description: 'Battery QC Batch SO-240706', active: true },
] as const

export const invoiceDefaults = [
  { id: 'fallback-inv-01', invoiceNo: '6907017', description: 'Invoice 6907017', active: true },
  { id: 'fallback-inv-02', invoiceNo: '6907018', description: 'Invoice 6907018', active: true },
  { id: 'fallback-inv-03', invoiceNo: '6907019', description: 'Invoice 6907019', active: true },
  { id: 'fallback-inv-04', invoiceNo: '6907020', description: 'Invoice 6907020', active: true },
  { id: 'fallback-inv-05', invoiceNo: '6907021', description: 'Invoice 6907021', active: true },
] as const

export const chargeChannelDefaults = [
  { id: 'fallback-ch-01', code: 'CH-A', name: 'Channel A', active: true },
  { id: 'fallback-ch-02', code: 'CH-B', name: 'Channel B', active: true },
  { id: 'fallback-ch-03', code: 'CH-C', name: 'Channel C', active: true },
  { id: 'fallback-ch-04', code: 'CH-D', name: 'Channel D', active: true },
] as const

export const chargeProgramDefaults = [
  { id: 'fallback-prg-01', code: 'PRG-NIGHT', name: 'Night', active: true },
  { id: 'fallback-prg-02', code: 'PRG-DAY', name: 'Day', active: true },
  { id: 'fallback-prg-03', code: 'PRG-BOOST', name: 'Boost', active: true },
] as const
