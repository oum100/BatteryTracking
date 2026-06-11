# BatteryTracking

Starter project for a battery inventory and lifecycle tracking app built with Nuxt 4, Prisma, and Tailwind CSS v4.

## Stack

- Nuxt 4
- `@prisma/client` `6.19.2`
- `tailwindcss` `^4.2.2`
- SQLite for local development

## Setup

```bash
npm install
```

## Database

Generate Prisma Client:

```bash
npm run db:generate
```

Create the local SQLite database from the starter schema:

```bash
npm run db:push
```

Open Prisma Studio:

```bash
npm run db:studio
```

## Development

Start the Nuxt dev server on `http://localhost:3000`:

```bash
npm run dev
```
