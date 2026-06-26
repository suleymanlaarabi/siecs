import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  site: 'https://suleymanlaarabi.github.io',
  base: '/siecs',
  integrations: [
    starlight({
      title: 'SIECS',
      description: 'Documentation for the SIECS entity component system.',
      sidebar: [
        { label: 'Overview', link: '/' },
        {
          label: 'Start Here',
          items: ['getting-started', 'theory', 'cookbook'],
        },
        {
          label: 'C API',
          items: [
            'components',
            'resources',
            'entities',
            'queries',
            'systems',
            'observers',
            'modules',
            'relations',
          ],
        },
        {
          label: 'Bindings',
          items: ['cpp', 'rust'],
        },
        {
          label: 'Addons',
          items: ['rest'],
        },
        {
          label: 'Production',
          items: ['performance', 'internals'],
        },
        {
          label: 'Reference',
          items: ['reference/api'],
        },
      ],
    }),
  ],
});
