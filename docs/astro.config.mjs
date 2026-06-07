import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

export default defineConfig({
  integrations: [
    starlight({
      title: 'SIECS',
      description: 'Documentation for the SIECS entity component system.',
      sidebar: [
        { label: 'Overview', link: '/' },
        {
          label: 'Guide',
          items: [
            'getting-started',
            'components',
            'entities',
            'queries',
            'observers',
            'relations',
            'internals',
          ],
        },
        {
          label: 'Reference',
          items: ['reference/api'],
        },
      ],
    }),
  ],
});
