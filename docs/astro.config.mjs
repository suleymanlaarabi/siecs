import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import languageExamples from './src/plugins/language-examples';

export default defineConfig({
  site: 'https://docs.siecs.dev',
  base: '/',
  markdown: {
    remarkPlugins: [languageExamples],
  },
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
          label: 'Core Concepts',
          items: [
            'components',
            'resources',
            'entities',
            'inheritance',
            'queries',
            'systems',
            'observers',
            'modules',
            'relations',
          ],
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
