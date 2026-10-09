import { Area, CartesianGrid, ComposedChart, Line, XAxis, YAxis } from 'recharts'

import {
  type ChartConfig,
  ChartContainer,
  ChartTooltip,
  ChartTooltipContent,
} from '@/components/ui/chart'
import type { HistoryPoint } from '@/lib/api'
import { formatDateTime, formatDay, formatDayTime, formatNumber, formatTime } from '@/lib/format'

export type Period = '1h' | '24h' | '7d' | '30d'

interface Props {
  points: HistoryPoint[]
  period: Period
  // Sem altura do sensor, o gráfico mostra a distância até a água
  mode: 'level' | 'distance'
}

const config = {
  avg: { label: 'Média', color: 'var(--color-sky-600)' },
  range: { label: 'Mín – máx', color: 'var(--color-sky-300)' },
} satisfies ChartConfig

const tickFormat: Record<Period, (iso: string) => string> = {
  '1h': formatTime,
  '24h': formatTime,
  '7d': formatDayTime,
  '30d': formatDay,
}

export function LevelChart({ points, period, mode }: Props) {
  const data = points.map((p) =>
    mode === 'level'
      ? { t: p.t, avg: p.level_avg, range: [p.level_min, p.level_max] }
      : { t: p.t, avg: p.distance_avg, range: [p.distance_min, p.distance_max] },
  )

  return (
    <ChartContainer config={config} className="aspect-auto h-72 w-full">
      <ComposedChart data={data} margin={{ left: 4, right: 12, top: 8 }}>
        <CartesianGrid vertical={false} />
        <XAxis
          dataKey="t"
          tickFormatter={tickFormat[period]}
          tickLine={false}
          axisLine={false}
          minTickGap={40}
        />
        <YAxis
          width={56}
          tickLine={false}
          axisLine={false}
          domain={['auto', 'auto']}
          // Distância menor = água mais alta; invertendo, o gráfico "sobe" quando o rio sobe
          reversed={mode === 'distance'}
          tickFormatter={(v: number) => formatNumber(v, 2)}
          unit=" m"
        />
        <ChartTooltip
          content={
            <ChartTooltipContent
              labelFormatter={(_, payload) => formatDateTime(payload[0]?.payload.t)}
              formatter={(value, name) => (
                <div className="flex w-full justify-between gap-4">
                  <span className="text-muted-foreground">
                    {config[name as keyof typeof config]?.label}
                  </span>
                  <span className="font-mono">
                    {Array.isArray(value)
                      ? `${formatNumber(value[0] as number)} – ${formatNumber(value[1] as number)} m`
                      : formatNumber(value as number, 3, 'm')}
                  </span>
                </div>
              )}
            />
          }
        />
        <Area
          dataKey="range"
          stroke="none"
          fill="var(--color-range)"
          fillOpacity={0.4}
          isAnimationActive={false}
        />
        <Line
          dataKey="avg"
          stroke="var(--color-avg)"
          strokeWidth={2}
          dot={false}
          isAnimationActive={false}
        />
      </ComposedChart>
    </ChartContainer>
  )
}
