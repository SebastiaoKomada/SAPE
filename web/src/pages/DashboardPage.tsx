import { useQuery } from '@tanstack/react-query'
import {
  ActivityIcon,
  BatteryMediumIcon,
  RadarIcon,
  RulerIcon,
  SunIcon,
  TrendingUpIcon,
  WifiIcon,
} from 'lucide-react'
import type { ReactNode } from 'react'
import { Link, useSearchParams } from 'react-router'

import { LevelChart, type Period } from '@/components/LevelChart'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Skeleton } from '@/components/ui/skeleton'
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table'
import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group'
import { api, type Device } from '@/lib/api'
import { formatDateTime, formatNumber, formatSigned, timeAgo } from '@/lib/format'
import { cn } from '@/lib/utils'

const PERIOD_MS: Record<Period, number> = {
  '1h': 3_600_000,
  '24h': 86_400_000,
  '7d': 7 * 86_400_000,
  '30d': 30 * 86_400_000,
}

export default function DashboardPage() {
  const [params, setParams] = useSearchParams()
  const devices = useQuery({ queryKey: ['devices'], queryFn: api.devices })

  if (devices.isPending) return <Skeleton className="h-96 w-full" />
  if (devices.isError) return <ErrorCard message={devices.error.message} />
  if (devices.data.length === 0) {
    return (
      <Card>
        <CardHeader>
          <CardTitle>Nenhum dispositivo cadastrado</CardTitle>
          <CardDescription>
            Cadastre a estação com o mesmo identificador do <code>DEVICE_ID</code> do firmware
            para começar a receber leituras.
          </CardDescription>
        </CardHeader>
        <CardContent>
          <Button asChild>
            <Link to="/dispositivos">Cadastrar dispositivo</Link>
          </Button>
        </CardContent>
      </Card>
    )
  }

  const selectedId = Number(params.get('device'))
  const device = devices.data.find((d) => d.id === selectedId) ?? devices.data[0]
  const period = (params.get('periodo') as Period | null) ?? '24h'

  const update = (key: string, value: string) =>
    setParams((p) => {
      p.set(key, value)
      return p
    })

  return (
    <div className="flex flex-col gap-6">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-xl font-semibold">{device.name}</h1>
          <p className="text-sm text-muted-foreground">
            {device.location ?? 'Sem localização'} · <code>{device.identifier}</code>
          </p>
        </div>
        {devices.data.length > 1 && (
          <Select value={String(device.id)} onValueChange={(v) => update('device', v)}>
            <SelectTrigger className="w-56">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              {devices.data.map((d) => (
                <SelectItem key={d.id} value={String(d.id)}>
                  {d.name}
                </SelectItem>
              ))}
            </SelectContent>
          </Select>
        )}
      </div>

      <StatCards device={device} />

      <Card>
        <CardHeader className="flex flex-row flex-wrap items-start justify-between gap-3">
          <div>
            <CardTitle>
              {device.sensor_height_m != null ? 'Nível do rio' : 'Distância até a água'}
            </CardTitle>
            <CardDescription>
              {device.sensor_height_m != null
                ? 'Média por intervalo, com a faixa entre mínimo e máximo'
                : 'Cadastre a altura do sensor para ver o nível do rio'}
            </CardDescription>
          </div>
          <ToggleGroup
            type="single"
            variant="outline"
            value={period}
            onValueChange={(v) => v && update('periodo', v)}
          >
            {(Object.keys(PERIOD_MS) as Period[]).map((p) => (
              <ToggleGroupItem key={p} value={p} className="px-3">
                {p}
              </ToggleGroupItem>
            ))}
          </ToggleGroup>
        </CardHeader>
        <CardContent>
          <HistoryChart device={device} period={period} />
        </CardContent>
      </Card>

      <RecentTable device={device} />
    </div>
  )
}

function StatCards({ device }: { device: Device }) {
  const stats = useQuery({
    queryKey: ['stats', device.id],
    queryFn: () => api.stats(device.id),
  })
  const latest = device.latest_measurement
  const s = stats.data
  const hasLevel = device.sensor_height_m != null
  const rate = s?.rise_rate_cm_h ?? null

  return (
    <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
      <StatCard
        icon={hasLevel ? <RulerIcon /> : <RadarIcon />}
        title={hasLevel ? 'Nível atual' : 'Distância até a água'}
        value={formatNumber(hasLevel ? s?.current_level_m : s?.current_distance_m, 2, 'm')}
        hint={hasLevel ? `Distância medida: ${formatNumber(s?.current_distance_m, 3, 'm')}` : 'Sem altura do sensor cadastrada'}
      />
      <StatCard
        icon={<TrendingUpIcon />}
        title="Taxa de elevação"
        value={formatSigned(rate, 1, 'cm/h')}
        valueClass={cn(rate != null && rate > 0 && 'text-amber-600', rate != null && rate < 0 && 'text-sky-600')}
        hint={
          s
            ? `Variação de ${formatSigned(s.level_change_m != null ? s.level_change_m * 100 : null, 1, 'cm')} na última ${s.window_min === 60 ? 'hora' : `janela de ${s.window_min} min`}`
            : ''
        }
      />
      <StatCard
        icon={<WifiIcon />}
        title="Conexão"
        value={
          <Badge variant={device.online ? 'default' : 'destructive'} className="text-sm">
            {device.online ? 'Online' : 'Offline'}
          </Badge>
        }
        hint={`Última mensagem ${timeAgo(device.last_seen_at)}`}
      />
      <StatCard
        icon={<BatteryMediumIcon />}
        title="Bateria"
        value={formatNumber(latest?.battery_v, 2, 'V')}
      />
      <StatCard icon={<SunIcon />} title="Painel solar" value={formatNumber(latest?.solar_v, 2, 'V')} />
      <StatCard
        icon={<ActivityIcon />}
        title="Sensor"
        value={
          latest ? (
            <Badge variant={latest.sensor_ok ? 'secondary' : 'destructive'} className="text-sm">
              {latest.sensor_ok ? 'Leitura OK' : 'Falha na leitura'}
            </Badge>
          ) : (
            '—'
          )
        }
        hint={s ? `${s.failed_in_window} falha(s) em ${s.readings_in_window} leitura(s) na janela` : ''}
      />
    </div>
  )
}

function StatCard(props: {
  icon: ReactNode
  title: string
  value: ReactNode
  hint?: string
  valueClass?: string
}) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between">
        <CardDescription>{props.title}</CardDescription>
        <span className="text-muted-foreground [&_svg]:size-4">{props.icon}</span>
      </CardHeader>
      <CardContent className="flex flex-col gap-1">
        <div className={cn('text-2xl font-semibold tabular-nums', props.valueClass)}>{props.value}</div>
        {props.hint && <p className="text-xs text-muted-foreground">{props.hint}</p>}
      </CardContent>
    </Card>
  )
}

function HistoryChart({ device, period }: { device: Device; period: Period }) {
  const history = useQuery({
    queryKey: ['history', device.id, period],
    queryFn: () => {
      const end = new Date()
      return api.history(device.id, new Date(end.getTime() - PERIOD_MS[period]), end)
    },
  })

  if (history.isPending) return <Skeleton className="h-72 w-full" />
  if (history.isError) return <p className="text-sm text-destructive">{history.error.message}</p>
  if (history.data.points.length === 0) {
    return (
      <div className="flex h-72 items-center justify-center text-sm text-muted-foreground">
        Nenhuma leitura válida neste período.
      </div>
    )
  }
  return (
    <LevelChart
      points={history.data.points}
      period={period}
      mode={device.sensor_height_m != null ? 'level' : 'distance'}
    />
  )
}

function RecentTable({ device }: { device: Device }) {
  const recent = useQuery({
    queryKey: ['recent', device.id],
    queryFn: () => api.recent(device.id),
  })

  return (
    <Card>
      <CardHeader>
        <CardTitle>Últimas leituras</CardTitle>
        <CardDescription>Inclui as leituras em que o sensor falhou</CardDescription>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Horário</TableHead>
              <TableHead className="text-right">Distância</TableHead>
              <TableHead className="text-right">Nível</TableHead>
              <TableHead>Sensor</TableHead>
              <TableHead className="text-right">Bateria</TableHead>
              <TableHead className="text-right">Solar</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {recent.data?.map((m) => (
              <TableRow key={m.id}>
                <TableCell>{formatDateTime(m.measured_at)}</TableCell>
                <TableCell className="text-right tabular-nums">{formatNumber(m.distance_m, 3, 'm')}</TableCell>
                <TableCell className="text-right tabular-nums">{formatNumber(m.river_level_m, 3, 'm')}</TableCell>
                <TableCell>
                  {m.sensor_ok ? (
                    <Badge variant="secondary">OK</Badge>
                  ) : (
                    <Badge variant="destructive">Falha</Badge>
                  )}
                </TableCell>
                <TableCell className="text-right tabular-nums">{formatNumber(m.battery_v, 2, 'V')}</TableCell>
                <TableCell className="text-right tabular-nums">{formatNumber(m.solar_v, 2, 'V')}</TableCell>
              </TableRow>
            ))}
            {recent.data?.length === 0 && (
              <TableRow>
                <TableCell colSpan={6} className="h-20 text-center text-muted-foreground">
                  Nenhuma leitura recebida ainda.
                </TableCell>
              </TableRow>
            )}
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  )
}

function ErrorCard({ message }: { message: string }) {
  return (
    <Card>
      <CardHeader>
        <CardTitle>Não foi possível carregar os dados</CardTitle>
        <CardDescription>{message}. Verifique se a API está rodando.</CardDescription>
      </CardHeader>
    </Card>
  )
}
