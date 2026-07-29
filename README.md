# BatteryTracking

Starter project for a battery inventory and lifecycle tracking app built with Nuxt 4, Prisma, and Tailwind CSS v4.

## Stack

- Nuxt 4
- `@prisma/client` `6.19.2`
- `tailwindcss` `^4.2.2`
- PostgreSQL 16 for local development

## Setup

```bash
npm install
```

## Database

Start PostgreSQL (Docker Desktop must be running):

```bash
docker compose up -d
```

The default `.env` connection uses the database started by this command:

```env
DATABASE_URL="postgresql://postgres:postgres@localhost:5432/batterytracking?schema=public"
```

Generate Prisma Client and apply the development migrations:

```bash
npm run db:generate
```

```bash
npm run db:migrate
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

Stop the local database when it is no longer needed:

```bash
docker compose down
```
